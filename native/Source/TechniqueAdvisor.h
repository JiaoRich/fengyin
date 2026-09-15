#pragma once

#include "DeviceProfile.h"
#include "MidiPerformanceSink.h"
#include "SwamPluginClassifier.h"

namespace fengyin
{
struct TechniqueAdvice
{
    bool relevantToInstrument = false;
    bool hardwareAvailable = false;
    const char* recommendedSource = "";
    const char* reason = "";
};

class TechniqueAdvisor
{
public:
    [[nodiscard]] static TechniqueAdvice advise(const DeviceProfile& device,
                                                 SwamFamily family,
                                                 PerformanceTechnique technique) noexcept
    {
        if (! isRelevant(family, technique))
            return { false, false, "", "当前乐器不需要此技巧" };

        switch (technique)
        {
            case PerformanceTechnique::vibrato:
                if (device.hasBiteSensor)
                    return { true, true, "吹嘴咬合", "可连续控制颤音深度" };
                if (device.hasMotionController)
                    return { true, true, "动作感应", "设备没有吹嘴咬合，推荐使用动作感应" };
                if (device.hasThumbController)
                    return { true, true, "拇指控制器", "设备没有吹嘴咬合，推荐使用拇指控制器" };
                break;
            case PerformanceTechnique::growl:
                if (device.hasThumbController)
                    return { true, true, "拇指控制器", "适合连续控制嘶吼强度" };
                if (device.hasAssignableButtons)
                    return { true, true, "功能键", "适合按住触发嘶吼音" };
                break;
            case PerformanceTechnique::flutter:
                if (device.hasAssignableButtons)
                    return { true, true, "功能键", "适合按住触发花舌" };
                if (device.hasThumbController)
                    return { true, true, "拇指控制器", "可用于控制花舌强度" };
                break;
            case PerformanceTechnique::count:
                break;
        }
        return { true, false, "点击识别", "未确认可用硬件，请操作希望使用的按键或控制器" };
    }

    [[nodiscard]] static bool isRelevant(SwamFamily family, PerformanceTechnique technique) noexcept
    {
        switch (technique)
        {
            case PerformanceTechnique::growl:
                return family == SwamFamily::saxophone || family == SwamFamily::brass;
            case PerformanceTechnique::vibrato:
                return family != SwamFamily::notSwam && family != SwamFamily::other;
            case PerformanceTechnique::flutter:
                return family == SwamFamily::saxophone || family == SwamFamily::brass
                    || family == SwamFamily::woodwind;
            case PerformanceTechnique::count:
                return false;
        }
        return false;
    }
};
}
