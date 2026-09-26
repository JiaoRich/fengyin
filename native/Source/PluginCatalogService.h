#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <functional>
#include <memory>
#include "SwamPluginClassifier.h"
#include "KongLibraryLocator.h"

namespace fengyin
{
class PluginCatalogService final : private juce::Thread
{
public:
    struct Progress
    {
        bool scanning = false;
        float fraction = 0.0f;
        juce::String currentFile;
        int pluginCount = 0;
        int failedCount = 0;
    };

    PluginCatalogService();
    ~PluginCatalogService() override;

    void startScan(juce::FileSearchPath paths, bool rescanExisting);
    void stopScan();
    [[nodiscard]] Progress getProgress() const;
    [[nodiscard]] juce::Array<juce::PluginDescription> getPlugins() const;
    [[nodiscard]] juce::Array<juce::PluginDescription> getSwamPlugins() const;
    [[nodiscard]] juce::FileSearchPath getRecommendedVst3Paths() const;
    [[nodiscard]] juce::Array<juce::var> getKongInstruments() const;
    void addScanPath(const juce::File& folder);
    bool setKongLibraryPath(const juce::File& folder);
    [[nodiscard]] juce::var getKongLibraryState() const;
    [[nodiscard]] juce::KnownPluginList& getKnownPlugins() noexcept { return knownPlugins; }

private:
    void run() override;
    juce::File getCatalogFile() const;
    juce::File getDeadMansPedalFile() const;
    void loadCatalog();
    void saveCatalog();
    void refreshKongLibrary();

    juce::AudioPluginFormatManager formatManager;
    juce::KnownPluginList knownPlugins;
    std::unique_ptr<juce::PluginDirectoryScanner> scanner;
    mutable juce::CriticalSection stateLock;
    Progress progress;
    bool shouldRescanExisting = false;
    juce::XmlElement programCatalog { "INSTRUMENT_PROGRAMS" };
    KongLibraryLocator::Result kongLibrary;
};
}
