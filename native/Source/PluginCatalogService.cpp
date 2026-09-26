#include "PluginCatalogService.h"
#include "KongInstrumentCatalog.h"
#include "KongProjectFile.h"

namespace fengyin
{
namespace
{
class IsolatedScanner final : public juce::KnownPluginList::CustomScanner
{
public:
    explicit IsolatedScanner(std::function<void(const juce::XmlElement&)> callback)
        : onScanned(std::move(callback)) {}

    bool findPluginTypesFor(juce::AudioPluginFormat&, juce::OwnedArray<juce::PluginDescription>& result,
                            const juce::String& path) override
    {
        juce::TemporaryFile report(".xml");
        juce::ChildProcess child;
        if (! child.start(juce::StringArray {
                juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),
                "--scan-plugin", path, report.getFile().getFullPathName() }, 0)) return false;
        const auto deadline = juce::Time::getMillisecondCounterHiRes() + 45000.0;
        while (child.isRunning())
        {
            if (shouldExit() || juce::Thread::currentThreadShouldExit()
                || juce::Time::getMillisecondCounterHiRes() >= deadline)
            {
                child.kill();
                return false;
            }
            juce::Thread::sleep(40);
        }
        auto xml = juce::XmlDocument::parse(report.getFile());
        if (child.getExitCode() != 0 || xml == nullptr || ! xml->hasTagName("PLUGIN_SCAN")) return false;
        for (const auto* element : xml->getChildIterator())
        {
            auto description = std::make_unique<juce::PluginDescription>();
            if (description->loadFromXml(*element)) result.add(description.release());
        }
        onScanned(*xml);
        return true;
    }
private:
    std::function<void(const juce::XmlElement&)> onScanned;
};
}

PluginCatalogService::PluginCatalogService()
    : juce::Thread("VST3 scanner")
{
    juce::addDefaultFormatsToManager(formatManager);
    loadCatalog();
    refreshKongLibrary();
    knownPlugins.setCustomScanner(std::make_unique<IsolatedScanner>([this](const juce::XmlElement& report)
    {
        const juce::ScopedLock lock(stateLock);
        for (const auto* bank : report.getChildWithTagNameIterator("PROGRAMS"))
        {
            for (int index = programCatalog.getNumChildElements(); --index >= 0;)
                if (programCatalog.getChildElement(index)->getStringAttribute("pluginId") == bank->getStringAttribute("pluginId"))
                    programCatalog.removeChildElement(programCatalog.getChildElement(index), true);
            programCatalog.addChildElement(new juce::XmlElement(*bank));
        }
    }));
}

PluginCatalogService::~PluginCatalogService()
{
    stopScan();
    saveCatalog();
}

void PluginCatalogService::startScan(juce::FileSearchPath paths, bool rescanExisting)
{
    stopScan();
    juce::AudioPluginFormat* vst3 = nullptr;
    for (auto* format : formatManager.getFormats())
        if (format->getName() == "VST3")
            vst3 = format;
    if (vst3 == nullptr)
        return;

    scanner = std::make_unique<juce::PluginDirectoryScanner>(knownPlugins, *vst3, std::move(paths),
                                                              true, getDeadMansPedalFile(), false);
    shouldRescanExisting = rescanExisting;
    {
        const juce::ScopedLock lock(stateLock);
        progress = {};
        progress.scanning = true;
        progress.pluginCount = knownPlugins.getNumTypes();
    }
    startThread(juce::Thread::Priority::low);
}

void PluginCatalogService::stopScan()
{
    signalThreadShouldExit();
    stopThread(4000);
    scanner.reset();
    const juce::ScopedLock lock(stateLock);
    progress.scanning = false;
}

PluginCatalogService::Progress PluginCatalogService::getProgress() const
{
    const juce::ScopedLock lock(stateLock);
    return progress;
}

juce::Array<juce::PluginDescription> PluginCatalogService::getPlugins() const
{
    const juce::ScopedLock lock(stateLock);
    if (progress.scanning)
        return {};
    return knownPlugins.getTypes();
}

juce::Array<juce::PluginDescription> PluginCatalogService::getSwamPlugins() const
{
    juce::Array<juce::PluginDescription> result;
    for (const auto& description : getPlugins())
        if (SwamPluginClassifier::classify(description.name.toStdString(),
                                           description.manufacturerName.toStdString()) != SwamFamily::notSwam)
            result.add(description);
    return result;
}

juce::Array<juce::var> PluginCatalogService::getKongInstruments() const
{
    const juce::ScopedLock lock(stateLock);
    juce::Array<juce::var> result;
    const auto plugins = knownPlugins.getTypes();
    for (const auto* bank : programCatalog.getChildIterator())
    {
        const auto pluginId = bank->getStringAttribute("pluginId");
        bool installed = false;
        for (const auto& plugin : plugins)
            if (plugin.createIdentifierString() == pluginId && juce::File(plugin.fileOrIdentifier).exists()) installed = true;
        if (! installed) continue;
        juce::StringArray added;
        for (const auto* program : bank->getChildWithTagNameIterator("PROGRAM"))
            if (const auto* definition = KongInstrumentCatalog::matchProgram(program->getStringAttribute("name")))
                if (! added.contains(definition->key))
                {
                    auto item = std::make_unique<juce::DynamicObject>();
                    item->setProperty("key", definition->key);
                    item->setProperty("name", juce::String::fromUTF8(definition->chineseName));
                    item->setProperty("program", program->getStringAttribute("name"));
                    item->setProperty("programIndex", program->getIntAttribute("index"));
                    item->setProperty("pluginId", pluginId);
                    item->setProperty("pluginName", bank->getStringAttribute("pluginName"));
                    result.add(juce::var(item.release()));
                    added.add(definition->key);
                }
    }
    return result;
}

juce::FileSearchPath PluginCatalogService::getRecommendedVst3Paths() const
{
    juce::FileSearchPath paths;
   #if JUCE_WINDOWS
    paths.add(juce::File("C:\\Program Files\\Common Files\\VST3"));
    const auto local = juce::SystemStats::getEnvironmentVariable("LOCALAPPDATA", {});
    if (local.isNotEmpty()) paths.add(juce::File(local).getChildFile("Programs/Common/VST3"));
   #elif JUCE_MAC
    paths.add(juce::File("/Library/Audio/Plug-Ins/VST3"));
    paths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                  .getChildFile("Library/Audio/Plug-Ins/VST3"));
   #endif
    const juce::FileSearchPath additional(getCatalogFile().getSiblingFile("scan-paths.txt").loadFileAsString());
    for (int index = 0; index < additional.getNumPaths(); ++index)
        if (additional[index].isDirectory()) paths.addIfNotAlreadyThere(additional[index]);
    return paths;
}

void PluginCatalogService::addScanPath(const juce::File& folder)
{
    if (! folder.isDirectory()) return;
    const auto file = getCatalogFile().getSiblingFile("scan-paths.txt");
    juce::FileSearchPath paths(file.loadFileAsString());
    paths.addIfNotAlreadyThere(folder);
    file.getParentDirectory().createDirectory();
    file.replaceWithText(paths.toString());
}

void PluginCatalogService::run()
{
    refreshKongLibrary();
    while (! threadShouldExit() && scanner != nullptr)
    {
        juce::String current;
        const auto hasMore = scanner->scanNextFile(! shouldRescanExisting, current);
        {
            const juce::ScopedLock lock(stateLock);
            progress.currentFile = current;
            progress.fraction = scanner->getProgress();
            progress.pluginCount = knownPlugins.getNumTypes();
            progress.failedCount = scanner->getFailedFiles().size();
        }
        if (! hasMore)
            break;
    }

    saveCatalog();
    const juce::ScopedLock lock(stateLock);
    progress.scanning = false;
    progress.fraction = 1.0f;
    progress.pluginCount = knownPlugins.getNumTypes();
}

void PluginCatalogService::refreshKongLibrary()
{
    const auto documents = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory);
    auto result = KongLibraryLocator::resolve(getCatalogFile().getSiblingFile("kong-library-path.txt"),
        documents.getChildFile("Kong Audio/config"), documents.getChildFile("Kong Audio Soundbank"));
    const juce::ScopedLock lock(stateLock);
    kongLibrary = std::move(result);
}

bool PluginCatalogService::setKongLibraryPath(const juce::File& folder)
{
    auto result = KongLibraryLocator::inspect(folder, "selected");
    if (! result.ready()) return false;
    const auto file = getCatalogFile().getSiblingFile("kong-library-path.txt");
    if (file.getParentDirectory().createDirectory().failed()) return false;
    juce::TemporaryFile temporary(file);
    if (! temporary.getFile().replaceWithText(folder.getFullPathName())
        || ! temporary.overwriteTargetFileWithTemporary()) return false;
    const juce::ScopedLock lock(stateLock);
    kongLibrary = std::move(result);
    return true;
}

juce::var PluginCatalogService::getKongLibraryState() const
{
    const juce::ScopedLock lock(stateLock);
    auto result = std::make_unique<juce::DynamicObject>();
    result->setProperty("path", kongLibrary.source.isEmpty() ? juce::String() : kongLibrary.directory.getFullPathName());
    result->setProperty("source", kongLibrary.source);
    result->setProperty("exists", kongLibrary.exists);
    result->setProperty("ready", kongLibrary.ready());
    result->setProperty("fileCount", kongLibrary.files.size());
    result->setProperty("truncated", kongLibrary.truncated);
    juce::Array<juce::var> instruments;
    for (const auto& file : kongLibrary.files)
    {
        const auto description = KongLibraryLocator::describe(file);
        auto item = std::make_unique<juce::DynamicObject>();
        item->setProperty("key", description.key);
        item->setProperty("name", description.chineseName);
        item->setProperty("file", description.originalName);
        item->setProperty("recognised", description.recognised);
        instruments.add(juce::var(item.release()));
    }
    result->setProperty("instruments", juce::var(instruments));
    return juce::var(result.release());
}

juce::MemoryBlock PluginCatalogService::createKongProjectForInstrument(const juce::String& instrumentName) const
{
    const juce::ScopedLock lock(stateLock);
    const auto wanted = instrumentName.trim();
    juce::String bestStem;
    int bestScore = -1;
    for (const auto& file : kongLibrary.files)
    {
        const auto description = KongLibraryLocator::describe(file);
        const auto candidate = description.chineseName.trim();
        int score = -1;
        if (candidate.equalsIgnoreCase(wanted)) score = 10000 + candidate.length();
        else if (wanted.containsIgnoreCase(candidate) || candidate.containsIgnoreCase(wanted))
            score = candidate.length();
        if (score > bestScore)
        {
            bestScore = score;
            bestStem = juce::File(file).getFileNameWithoutExtension();
        }
    }
    return bestStem.isNotEmpty() ? KongProjectFile::create(bestStem) : juce::MemoryBlock();
}

juce::File PluginCatalogService::getCatalogFile() const
{
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("FengYin").getChildFile("plugins.xml");
}

juce::File PluginCatalogService::getDeadMansPedalFile() const
{
    return getCatalogFile().getSiblingFile("plugin-scan-in-progress.txt");
}

void PluginCatalogService::loadCatalog()
{
    const auto file = getCatalogFile();
    if (auto xml = juce::XmlDocument::parse(file))
        knownPlugins.recreateFromXml(*xml);
    if (auto xml = juce::XmlDocument::parse(file.getSiblingFile("instrument-programs.xml")))
        programCatalog = *xml;
}

void PluginCatalogService::saveCatalog()
{
    const auto file = getCatalogFile();
    file.getParentDirectory().createDirectory();
    if (auto xml = knownPlugins.createXml())
        xml->writeTo(file);
    const juce::ScopedLock lock(stateLock);
    programCatalog.writeTo(file.getSiblingFile("instrument-programs.xml"));
}
}
