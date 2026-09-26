#pragma once

#include <array>
#include <juce_core/juce_core.h>

namespace fengyin
{
struct ContainerPluginAdapterDefinition
{
    const char* id;
    const char* brand;
    const char* displayName;
    bool enabled;
};

// A container plug-in hosts instruments selected inside its own editor.  Scanning
// the plug-in or its sample files must never be treated as creating a playable
// instrument.  Each adapter shares the same create/capture/restore workflow.
class ContainerPluginAdapterCatalog final
{
public:
    static const std::array<ContainerPluginAdapterDefinition, 4>& all()
    {
        static const std::array<ContainerPluginAdapterDefinition, 4> adapters {{
            { "kong-v3", "kong", "QinEngineV3", true },
            { "kontakt", "kontakt", "Kontakt", false },
            { "falcon", "falcon", "Falcon", false },
            { "three-body", "three-body", "三体音源", false }
        }};
        return adapters;
    }

    static const ContainerPluginAdapterDefinition* find(const juce::String& id)
    {
        for (const auto& adapter : all())
            if (id == adapter.id) return &adapter;
        return nullptr;
    }
};
}
