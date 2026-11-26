#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "KongInstrumentCatalog.h"
#include "SwamPluginClassifier.h"

namespace fengyin
{
// Presentation only. Never use this label as a plug-in identifier or to restore state.
inline juce::String instrumentDisplayName(const juce::PluginDescription& plugin)
{
    if (plugin.name == "FengYin SF2" && plugin.manufacturerName == "FengYin")
        return juce::String::fromUTF8("sf2格式音源");
    const auto brand = SupportedInstrumentClassifier::classify(
        plugin.name, plugin.manufacturerName, plugin.fileOrIdentifier);
    if (brand == SupportedInstrumentClassifier::Brand::kong)
        return juce::String::fromUTF8("民乐引擎");
    if (brand == SupportedInstrumentClassifier::Brand::swam)
    {
        const auto name = juce::String::fromUTF8(
            SwamPluginClassifier::instrumentChineseName(plugin.name.toStdString()));
        return name.containsIgnoreCase("SWAM") ? juce::String::fromUTF8("西洋乐器") : name;
    }
    return plugin.name;
}
}
