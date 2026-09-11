#include "PluginCatalogService.h"

namespace fengyin
{
PluginCatalogService::PluginCatalogService()
    : juce::Thread("VST3 scanner")
{
    juce::addDefaultFormatsToManager(formatManager);
    loadCatalog();
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

juce::FileSearchPath PluginCatalogService::getRecommendedVst3Paths() const
{
    juce::FileSearchPath paths;
   #if JUCE_WINDOWS
    paths.add(juce::File("C:\\Program Files\\Common Files\\VST3"));
   #elif JUCE_MAC
    paths.add(juce::File("/Library/Audio/Plug-Ins/VST3"));
    paths.add(juce::File::getSpecialLocation(juce::File::userHomeDirectory)
                  .getChildFile("Library/Audio/Plug-Ins/VST3"));
   #endif
    return paths;
}

void PluginCatalogService::run()
{
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
}

void PluginCatalogService::saveCatalog()
{
    const auto file = getCatalogFile();
    file.getParentDirectory().createDirectory();
    if (auto xml = knownPlugins.createXml())
        xml->writeTo(file);
}
}
