#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace fengyin
{
struct DeviceProfile
{
    std::string id;
    std::string displayName;
    int breathController = 2;
    int expressionController = 11;
    int pitchBendSemitones = 2;
    float breathCurve = 0.9f;
    float smoothing = 0.28f;
    bool hasBiteSensor = false;
    bool hasThumbController = false;
    bool hasAssignableButtons = false;
    bool hasMotionController = false;
    bool breathDrivenVelocity = true;
    bool safeOnsetProtection = true;
};

class DeviceProfileMatcher
{
public:
    [[nodiscard]] static DeviceProfile match(std::string_view deviceName)
    {
        const auto name = lower(deviceName);
        if (contains(name, "aerophone") || contains(name, "ae-"))
            return { "roland-aerophone", "Roland Aerophone", 2, 11, 2, 0.9f, 0.26f,
                     true, true, true, true, true, true };
        if (contains(name, "yds-") || contains(name, "yamaha"))
            return { "yamaha-yds", "Yamaha YDS", 11, 11, 2, 0.95f, 0.28f,
                     false, true, true, true, true, true };
        if (contains(name, "ewi") || contains(name, "akai"))
            return { "akai-ewi", "Akai EWI", 2, 11, 2, 0.85f, 0.24f,
                     true, true, true, false, true, true };
        if (contains(name, "sylphyo") || contains(name, "aodyo"))
            return { "aodyo-sylphyo", "Aodyo Sylphyo", 2, 11, 2, 0.9f, 0.25f,
                     false, true, true, true, true, true };
        if (contains(name, "wudi") || contains(name, "wu di") || contains(name, "无笛"))
            return { "wudi", "无笛电吹管", 2, 11, 2, 0.9f, 0.24f,
                     false, true, true, false, true, true };
        return { "generic-wind-controller", "通用电吹管", 2, 11, 2, 0.9f, 0.28f,
                 false, false, false, false, true, true };
    }

private:
    static std::string lower(std::string_view value)
    {
        std::string result(value);
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return result;
    }

    static bool contains(const std::string& text, std::string_view pattern)
    {
        return text.find(pattern) != std::string::npos;
    }
};
}
