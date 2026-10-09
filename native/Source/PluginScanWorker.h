#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "KongInstrumentCatalog.h"

// Runs only in the disposable scan process. No audio device is opened here.
class PluginScanWorker final : private juce::Timer
{
public:
    PluginScanWorker(const juce::String& path, const juce::File& destination)
        : output(destination), document("PLUGIN_SCAN")
    {
        juce::addDefaultFormatsToManager(manager);
        startTimer(40000);
        for (auto* format : manager.getFormats())
            if (format->getName() == "VST3") format->findAllTypesForFile(types, path);
        for (const auto* type : types)
            document.addChildElement(type->createXml().release());
        // Discovery survives optional instance validation failure or timeout.
        document.setAttribute("validation", "pending");
        document.writeTo(output);
        next();
    }

private:
    void next()
    {
        while (position < types.size())
        {
            const auto description = *types[position++];
            const auto freshAir = description.name.containsIgnoreCase("Fresh Air");
            if (fengyin::SupportedInstrumentClassifier::classify(description.name, description.manufacturerName, description.fileOrIdentifier)
                != fengyin::SupportedInstrumentClassifier::Brand::kong && !freshAir) continue;
            manager.createPluginInstanceAsync(description, 48000.0, 128,
                [this, description](std::unique_ptr<juce::AudioPluginInstance> instance, const juce::String& error)
                {
                    if (description.name.containsIgnoreCase("Fresh Air") && instance == nullptr)
                    {
                        juce::Logger::writeToLog("Fresh Air instance validation failed: " + error);
                        document.setAttribute("error", error);
                        document.writeTo(output);
                        document.setAttribute("validation", "failed");
                        juce::MessageManager::callAsync([this] { next(); });
                        return;
                    }
                    auto* bank = document.createNewChildElement("PROGRAMS");
                    bank->setAttribute("pluginId", description.createIdentifierString());
                    bank->setAttribute("pluginName", description.name);
                    bank->setAttribute("error", error);
                    if (instance != nullptr)
                    {
                        bank->setAttribute("programCount", instance->getNumPrograms());
                        bank->setAttribute("outputBusCount", instance->getBusCount(false));
                        bank->setAttribute("outputChannels", instance->getTotalNumOutputChannels());
                        for (int bus = 0; bus < instance->getBusCount(false); ++bus)
                        {
                            const auto* outputBus = instance->getBus(false, bus);
                            auto* node = bank->createNewChildElement("OUTPUT_BUS");
                            node->setAttribute("name", outputBus->getName());
                            node->setAttribute("enabled", outputBus->isEnabled());
                            node->setAttribute("defaultChannels", outputBus->getDefaultLayout().size());
                        }
                        for (const auto* parameter : instance->getParameters())
                        {
                            auto* node = bank->createNewChildElement("PARAMETER");
                            node->setAttribute("name", parameter->getName(128));
                            node->setAttribute("steps", parameter->getNumSteps());
                        }
                        for (int index = 0; index < instance->getNumPrograms(); ++index)
                        {
                            const auto name = instance->getProgramName(index);
                            if (name.isEmpty()) continue;
                            auto* program = bank->createNewChildElement("PROGRAM");
                            program->setAttribute("index", index);
                            program->setAttribute("name", name);
                        }
                    }
                    instance.reset();
                    juce::MessageManager::callAsync([this] { next(); });
                });
            return;
        }
        stopTimer();
        if (!document.hasAttribute("error")) document.setAttribute("validation", "complete");
        const auto success = document.writeTo(output);
        juce::JUCEApplication::getInstance()->setApplicationReturnValue(success ? 0 : 2);
        juce::JUCEApplication::getInstance()->quit();
    }

    void timerCallback() override
    {
        juce::JUCEApplication::getInstance()->setApplicationReturnValue(3);
        juce::JUCEApplication::getInstance()->quit();
    }

    juce::AudioPluginFormatManager manager;
    juce::OwnedArray<juce::PluginDescription> types;
    juce::File output;
    juce::XmlElement document;
    int position = 0;
};
