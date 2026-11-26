#include "Sf2Resource.h"
#include "Sf2Voice.h"
#include <iostream>
#include <set>

#if JUCE_WINDOWS
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    if (argc != 4) return 2;
   #if JUCE_WINDOWS
    const auto argument=[&](int i){return juce::String(argv[i]);};
   #else
    const auto argument=[&](int i){return juce::String::fromUTF8(argv[i]);};
   #endif
    if (argument(1) != "--inspect") return 2;
    const juce::File file{argument(2)}, output{argument(3)};
    auto* object = new juce::DynamicObject(); juce::var report(object);
    object->setProperty("version", 1);
    object->setProperty("success", false);
    try {
        const auto checked = fengyin::sf2::validateFile(file);
        if (checked.failed()) throw std::runtime_error(checked.getErrorMessage().toStdString());
        fengyin::sf2::Voice voice(48000.);
        juce::Array<juce::var> presets;
        std::set<std::pair<int,int>> identities;
        for (const auto& p : voice.load(file.getFullPathName().toStdString())) {
            if (!identities.emplace(p.bank, p.program).second) throw std::runtime_error("Duplicate bank/program");
            auto* entry = new juce::DynamicObject();
            entry->setProperty("bank", p.bank); entry->setProperty("program", p.program);
            entry->setProperty("name", juce::String::fromUTF8(p.name.c_str()));
            presets.add(juce::var(entry));
        }
        if(presets.isEmpty())throw std::runtime_error("SF2 没有可用预设");
        if(!voice.select(int(presets[0]["bank"]),int(presets[0]["program"]),2))throw std::runtime_error("SF2 预设不能加载");
        const uint8_t breath[]{0xb0,11,80},note[]{0x90,60,80};voice.midi(breath,3);voice.midi(note,3);
        float left[128]{},right[128]{};
        for(int block=0;block<64;++block){voice.render(left,right,128);for(int i=0;i<128;++i)
            if(!std::isfinite(left[i])||!std::isfinite(right[i]))throw std::runtime_error("SF2 试渲染出现无效采样");}
        voice.reset(2);
        object->setProperty("renderValidated", true);
        object->setProperty("presets", presets);
        object->setProperty("sha256", juce::SHA256(file).toHexString());
        object->setProperty("engine", fluid_version_str());
        object->setProperty("success", true);
    } catch (const std::exception& error) {
        object->setProperty("error", juce::String::fromUTF8(error.what()));
    }
    if (!output.replaceWithText(juce::JSON::toString(report))) return 3;
    return bool(report["success"]) ? 0 : 1;
}
