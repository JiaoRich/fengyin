#pragma once

#include "DeviceProfile.h"
#include "IntelligentTechniqueProcessor.h"
#include "SwamPluginClassifier.h"

namespace fengyin
{
struct TechniqueAdvice
{
    bool relevantToInstrument = false;
    bool hardwareAvailable = false;
    const char* recommendedSource = "";
    const char* reason = "";
    TechniqueControlMode defaultMode = TechniqueControlMode::automatic;
    float defaultStrength = 0.5f;
    bool featured = false;
};

class TechniqueAdvisor
{
public:
    [[nodiscard]] static TechniqueAdvice advise(const DeviceProfile& device,
                                                 SwamFamily family,
                                                 PerformanceTechnique technique) noexcept
    {
        if (! isRelevant(family, technique))
            return { false, false, "", "当前乐器不需要此技巧", TechniqueControlMode::off, 0.0f, false };

        const auto breathCapable = IntelligentTechniqueProcessor::canUseBreath(technique);
        switch (technique)
        {
            case PerformanceTechnique::vibrato:
                if (device.hasBiteSensor)
                    return { true, true, "吹嘴咬合", "连续控制颤音深度", TechniqueControlMode::hardware, 0.58f, true };
                if (device.hasMotionController)
                    return { true, true, "动作感应", "长音时自然控制颤音", TechniqueControlMode::hardware, 0.55f, true };
                if (device.hasThumbController)
                    return { true, true, "拇指控制器", "连续控制颤音深度", TechniqueControlMode::hardware, 0.55f, true };
                return { true, false, "自动渐入", "长音时自动加入自然颤音", TechniqueControlMode::automatic, 0.55f, true };

            case PerformanceTechnique::growl:
                if (device.id == "yamaha-yds")
                    return { true, false, "气息智能", "强吹到高气息区后平滑渐入", TechniqueControlMode::breath, 0.50f, true };
                if (device.hasThumbController)
                    return { true, true, "拇指控制器＋气息", "控制器允许，气息决定嘶吼深浅", TechniqueControlMode::hybrid, 0.50f, true };
                if (device.hasAssignableButtons)
                    return { true, true, "功能键＋气息", "按住允许，气息决定嘶吼深浅", TechniqueControlMode::hybrid, 0.50f, true };
                return { true, false, "气息智能", "强吹到高气息区后平滑渐入", TechniqueControlMode::breath, 0.50f, true };

            case PerformanceTechnique::flutter:
                if (device.hasAssignableButtons)
                    return { true, true, "功能键＋气息", "按住允许，气息决定花舌强度", TechniqueControlMode::hybrid, 0.36f, true };
                return { true, false, "气息动作", "明显快速强吹后触发", TechniqueControlMode::breath, 0.34f, true };

            case PerformanceTechnique::portamento:
            case PerformanceTechnique::legato:
                return { true, false, "音符关系智能", "根据换音速度、音程和衔接自动判断", TechniqueControlMode::automatic, 0.48f, true };

            case PerformanceTechnique::fall:
                return { true, device.hasAssignableButtons,
                         device.hasAssignableButtons ? "功能键" : "句尾动作",
                         device.hasAssignableButtons ? "按键触发最准确" : "根据句尾收气辅助判断",
                         device.hasAssignableButtons ? TechniqueControlMode::hardware : TechniqueControlMode::automatic,
                         0.40f, true };

            case PerformanceTechnique::overblow:
                return { true, false, "气息智能", "仅在强奏区逐渐增加泛音", TechniqueControlMode::breath, 0.35f, false };
            case PerformanceTechnique::breathNoise:
                return { true, false, "气息智能", "弱吹区增加空气质感", TechniqueControlMode::breath, 0.30f, false };
            case PerformanceTechnique::bowPressure:
                return { true, false, "气息连续控制", "气息对应弓压和力度", TechniqueControlMode::breath, 0.48f, true };

            case PerformanceTechnique::alternateFingering:
            case PerformanceTechnique::mute:
            case PerformanceTechnique::pizzicato:
                if (device.hasAssignableButtons)
                    return { true, true, "功能键", "按下切换，松开恢复", TechniqueControlMode::hardware, 0.50f, false };
                return { true, false, "暂不启用", "当前设备没有适合的独立控制器", TechniqueControlMode::off, 0.0f, false };

            case PerformanceTechnique::halfValve:
            case PerformanceTechnique::tremolo:
                if (device.hasThumbController)
                    return { true, true, "拇指控制器", "连续控制技巧深度", TechniqueControlMode::hardware, 0.40f, false };
                if (device.hasAssignableButtons)
                    return { true, true, "功能键", "按住触发", TechniqueControlMode::hardware, 0.40f, false };
                return { true, false, "暂不启用", "当前设备没有合适的控制器", TechniqueControlMode::off, 0.0f, false };

            case PerformanceTechnique::count:
                break;
        }
        return { true, false, breathCapable ? "气息智能" : "自动推荐",
                 "按当前设备和乐器自动配置",
                 breathCapable ? TechniqueControlMode::breath : TechniqueControlMode::automatic,
                 0.5f, false };
    }

    [[nodiscard]] static bool isRelevant(SwamFamily family, PerformanceTechnique technique) noexcept
    {
        switch (technique)
        {
            case PerformanceTechnique::growl:
                return family == SwamFamily::saxophone || family == SwamFamily::brass
                    || family == SwamFamily::woodwind;
            case PerformanceTechnique::vibrato:
                return family != SwamFamily::notSwam && family != SwamFamily::other;
            case PerformanceTechnique::flutter:
                return family == SwamFamily::saxophone || family == SwamFamily::brass
                    || family == SwamFamily::woodwind;
            case PerformanceTechnique::portamento:
                return family != SwamFamily::notSwam && family != SwamFamily::other;
            case PerformanceTechnique::fall:
                return family == SwamFamily::saxophone || family == SwamFamily::brass;
            case PerformanceTechnique::overblow:
            case PerformanceTechnique::breathNoise:
            case PerformanceTechnique::alternateFingering:
                return family == SwamFamily::saxophone || family == SwamFamily::woodwind;
            case PerformanceTechnique::mute:
            case PerformanceTechnique::halfValve:
                return family == SwamFamily::brass;
            case PerformanceTechnique::legato:
            case PerformanceTechnique::bowPressure:
            case PerformanceTechnique::pizzicato:
            case PerformanceTechnique::tremolo:
                return family == SwamFamily::strings;
            case PerformanceTechnique::count:
                return false;
        }
        return false;
    }

    [[nodiscard]] static const char* chineseName(PerformanceTechnique technique) noexcept
    {
        switch (technique)
        {
            case PerformanceTechnique::growl: return "嘶吼音";
            case PerformanceTechnique::vibrato: return "颤音／揉弦";
            case PerformanceTechnique::flutter: return "花舌";
            case PerformanceTechnique::portamento: return "滑音";
            case PerformanceTechnique::fall: return "Doit／Fall";
            case PerformanceTechnique::overblow: return "泛音与超吹";
            case PerformanceTechnique::breathNoise: return "气声";
            case PerformanceTechnique::alternateFingering: return "替代指法音色";
            case PerformanceTechnique::mute: return "弱音器";
            case PerformanceTechnique::halfValve: return "半按阀效果";
            case PerformanceTechnique::legato: return "连奏";
            case PerformanceTechnique::bowPressure: return "弓压";
            case PerformanceTechnique::pizzicato: return "拨奏";
            case PerformanceTechnique::tremolo: return "颤弓";
            case PerformanceTechnique::count: break;
        }
        return "演奏技巧";
    }
};
}
