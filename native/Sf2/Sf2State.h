#pragma once
#include "Sf2Resource.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <optional>

namespace fengyin::sf2
{
struct State
{
    juce::String hash, name;
    juce::int64 bytes = 0;
    int bank = -1, program = -1, bend = 2;
    float gain = 1.f;
    bool ready = false, prepared = false;
    juce::String error;
    bool hasResource() const { return validHash(hash) && bank >= 0 && program >= 0; }
    juce::MemoryBlock encode() const
    {
        juce::XmlElement xml("FENGYIN_SF2");
        xml.setAttribute("version", 1); xml.setAttribute("engine", "2.6.3");
        xml.setAttribute("bytes", int(bytes)); xml.setAttribute("hash", hash); xml.setAttribute("name", name);
        xml.setAttribute("bank", bank); xml.setAttribute("program", program);
        xml.setAttribute("bend", bend); xml.setAttribute("gain", double(gain));
        xml.setAttribute("ready", ready); xml.setAttribute("prepared", prepared); xml.setAttribute("error", error);
        juce::MemoryBlock result; juce::AudioProcessor::copyXmlToBinary(xml, result); return result;
    }
    static std::optional<State> decode(const void* data, int size, int depth = 0)
    {
        if (size <= 0 || size > 1024 * 1024 || depth > 1) return {};
        auto xml = juce::AudioProcessor::getXmlFromBinary(data, size);
        if (!xml) return {};
        // JUCE VST3 hosts wrap the component's exact state. Read both forms;
        // never search arbitrary binary data for a coincidental tag.
        if (!xml->hasTagName("FENGYIN_SF2")) {
            auto* component = xml->getChildByName("IComponent"); juce::MemoryBlock body;
            if (depth || !component || !body.fromBase64Encoding(component->getAllSubText())) return {};
            return decode(body.getData(), int(body.getSize()), depth + 1);
        }
        if (xml->getIntAttribute("version") != 1 || xml->getStringAttribute("engine") != "2.6.3") return {};
        State s;
        s.bytes = xml->getIntAttribute("bytes");
        s.hash = xml->getStringAttribute("hash"); s.name = xml->getStringAttribute("name").substring(0, 256);
        s.bank = xml->getIntAttribute("bank", -1); s.program = xml->getIntAttribute("program", -1);
        s.bend = xml->getIntAttribute("bend", -1); s.gain = float(xml->getDoubleAttribute("gain", -1));
        s.ready = xml->getBoolAttribute("ready"); s.prepared = xml->getBoolAttribute("prepared");
        s.error = xml->getStringAttribute("error").substring(0, 1024);
        if (s.bytes < 0 || s.bytes > maximumFileBytes || (!s.hash.isEmpty() && !validHash(s.hash)) || s.bank < -1 || s.bank > 16383 || s.program < -1 || s.program > 127
            || s.bend < 0 || s.bend > 24 || !std::isfinite(s.gain) || s.gain < 0 || s.gain > 2) return {};
        return s;
    }
};
}
