#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace fengyin
{
// Diagnostic reads must include routing, model and technique parameters too.
// Never reuse captureToneParameters(), whose preset-safety filters omit them.
struct ToneDiagnostic
{
    static juce::String chineseName(const juce::String& english)
    {
        const auto key = english.toLowerCase().removeCharacters(" ._-/()[]");
        struct Name { const char* key; const char* chinese; };
        static const Name names[] {
            { "timbre", "音色色彩" }, { "brightness", "明亮度" },
            { "formant", "共振峰" }, { "reedstiffness", "哨片硬度" },
            { "reedhardness", "哨片硬度" }, { "attack", "起音" },
            { "attacktime", "起音时间" }, { "attackshape", "起音形状" },
            { "harmonics", "泛音" }, { "harmonicstructure", "泛音结构" },
            { "subharm", "次谐波" }, { "modalresgain", "模态共振增益" },
            { "timbralcorrection", "泛音塑形开关" },
            { "harmonicagain", "泛音 A 增益" }, { "harma", "泛音 A 阶次" },
            { "harmonicbgain", "泛音 B 增益" }, { "harmb", "泛音 B 阶次" },
            { "compressor", "压缩量" }, { "eqenabled", "均衡器开关" },
            { "eqlowgain", "EQ 低频增益" }, { "eqmidgain", "EQ 中频增益" },
            { "eqmidfreq", "EQ 中频频率" }, { "eqhighgain", "EQ 高频增益" },
            { "randomdynamic", "随机动态" }, { "dynamicpitch", "动态音高" },
            { "dynamicharmonic", "动态泛音" }, { "releasetime", "释放时间" },
            { "breathyppp", "极弱气声" },
            { "keynoise", "按键噪声" }, { "mechanicalnoise", "机械噪声" },
            { "resonance", "共鸣" }, { "breathnoise", "气声噪声" },
            { "breathnoiseamount", "气声噪声量" }, { "expression", "表情力度" },
            { "breath", "气息" }, { "growl", "嘶吼" },
            { "vibrato", "颤音" }, { "vibratodepth", "颤音深度" },
            { "vibratofrequency", "颤音频率" }, { "vibratorate", "颤音速率" },
            { "flutter", "花舌" }, { "fluttertongue", "花舌" },
            { "portamento", "滑音" }, { "legato", "连奏" },
            { "pitchbend", "弯音" }, { "pitchbendrange", "弯音范围" },
            { "instrument", "乐器" }, { "instrumentmodel", "乐器型号" },
            { "model", "型号" }, { "volume", "音量" }, { "gain", "增益" },
            { "pan", "声像" }, { "reverb", "混响" }, { "reverbmix", "混响混合" },
            { "transpose", "移调" }, { "tuning", "调音" },
            { "velocity", "音符力度" }, { "aftertouch", "触后" }
        };
        for (const auto& name : names)
            if (key == name.key) return juce::String::fromUTF8(name.chinese);
        return {}; // Preserve unknown names verbatim; do not guess a translation.
    }

    static juce::var capture(juce::AudioProcessor& processor)
    {
        auto result = std::make_unique<juce::DynamicObject>();
        result->setProperty("schema", "fengyin.tone-diagnostic.v1");
        result->setProperty("capturedAt", juce::Time::getCurrentTime().toISO8601(true));
        result->setProperty("parameterScope", "all host-exposed parameters; hidden settings may exist only in plugin state");
        result->setProperty("snapshotNote", "Point-in-time read, not an atomic snapshot of external MIDI or plugin editor changes");
        // Message-thread only. No detach, reset, preset application or setter.
        const juce::ScopedLock lock(processor.getCallbackLock());
        juce::MemoryBlock state;
        processor.getStateInformation(state);
        result->setProperty("stateEncoding", "base64-rfc4648");
        result->setProperty("stateBytes", static_cast<juce::int64>(state.getSize()));
        result->setProperty("stateBase64", juce::Base64::toBase64(state.getData(), state.getSize()));
        result->setProperty("stateAvailable", ! state.isEmpty());
        juce::Array<juce::var> rows;
        const auto& parameters = processor.getParameters();
        for (int index = 0; index < parameters.size(); ++index)
        {
            const auto* parameter = parameters[index];
            if (parameter == nullptr) continue;
            auto row = std::make_unique<juce::DynamicObject>();
            const auto english = parameter->getName(1024);
            const auto chinese = chineseName(english);
            const auto value = parameter->getValue();
            row->setProperty("index", index);
            if (const auto* identified = dynamic_cast<const juce::AudioProcessorParameterWithID*>(parameter))
                row->setProperty("id", identified->paramID);
            else
                row->setProperty("id", "index:" + juce::String(index));
            row->setProperty("nameEnglish", english);
            row->setProperty("nameChinese", chinese.isNotEmpty() ? chinese : juce::String::fromUTF8("待核对译名"));
            row->setProperty("translationVerified", chinese.isNotEmpty());
            row->setProperty("normalisedValue", static_cast<double>(value));
            row->setProperty("displayValue", parameter->getText(value, 1024));
            row->setProperty("unit", parameter->getLabel());
            row->setProperty("defaultNormalisedValue", static_cast<double>(parameter->getDefaultValue()));
            row->setProperty("isDiscrete", parameter->isDiscrete());
            row->setProperty("numSteps", parameter->getNumSteps());
            rows.add(juce::var(row.release()));
        }
        result->setProperty("parameterCount", rows.size());
        result->setProperty("parameters", rows);
        return juce::var(result.release());
    }
};
}
