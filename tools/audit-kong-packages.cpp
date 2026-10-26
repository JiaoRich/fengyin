#include "TonePackage.h"
#include "KongProjectFile.h"
#include "PublishedToneCatalog.h"
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 3) return 2;
    const juce::File root(juce::String::fromUTF8(argv[1]));
    const juce::File output(juce::String::fromUTF8(argv[2]));
    juce::Array<juce::var> rows;
    for (const auto& definition : fengyin::publishedToneCatalog) {
        if (definition.kind != fengyin::PublishedToneKind::kongPackage) continue;
        const auto filename = juce::String(definition.assetName.data());
        const auto file = root.getChildFile("assets/tone-packages").getChildFile(filename);
        juce::String error;
        const auto preset = fengyin::TonePackage::read(file, error);
        if (!preset) { std::cerr << filename << ": " << error << '\n'; return 3; }
        const auto state = fengyin::KongProjectFile::readPluginComponentState(preset->samplerState);
        if (!state.isValid()) return 4;
        auto* row = new juce::DynamicObject();
        row->setProperty("name", juce::String::fromUTF8(definition.name.data()));
        row->setProperty("file", filename);
        row->setProperty("sha256", juce::SHA256(file).toHexString());
        row->setProperty("instrumentKey", preset->instrumentKey);
        row->setProperty("otherChannel", preset->kongTechniques.otherChannel);
        row->setProperty("otherKeyswitchIndex", preset->kongTechniques.otherKeyswitchIndex);
        juce::Array<juce::var> slots;
        for (auto slot : state.getChildWithName("PresetList")) {
            auto* item = new juce::DynamicObject();
            for (const auto* name : {"KAI", "Preset", "IndexSelectedKeyswitch", "KeyswitchNames"})
                item->setProperty(name, slot.getProperty(name));
            item->setProperty("channel", int(slot.getProperty("MIDI", -1)) + 1);
            const auto names = slot.getProperty("KeyswitchNames");
            const int selected = slot.getProperty("IndexSelectedKeyswitch", -1);
            item->setProperty("selectedArticulation", names.isArray() && selected >= 0 && selected < names.size()
                ? names[selected].toString() : juce::String("unconfirmed"));
            slots.add(juce::var(item));
        }
        row->setProperty("slots", slots);
        rows.add(juce::var(row));
    }
    if (rows.size() != 13 || !output.getParentDirectory().createDirectory()) return 5;
    if (!output.replaceWithText(juce::JSON::toString(rows))) return 6;
    std::cout << "Audited " << rows.size() << " current Qin packages: " << output.getFullPathName() << '\n';
    return 0;
}
