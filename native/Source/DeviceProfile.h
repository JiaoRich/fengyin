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
};

class DeviceProfileMatcher
{
public:
    [[nodiscard]] static DeviceProfile match(std::string_view deviceName)
    {
        const auto name = lower(deviceName);
        if (contains(name, "aerophone") || contains(name, "ae-"))
            return { "roland-aerophone", "Roland Aerophone", 2, 11, 2, 0.9f, 0.26f };
        if (contains(name, "yds-") || contains(name, "yamaha"))
            return { "yamaha-yds", "Yamaha YDS", 2, 11, 2, 0.95f, 0.28f };
        if (contains(name, "ewi") || contains(name, "akai"))
            return { "akai-ewi", "Akai EWI", 2, 11, 2, 0.85f, 0.24f };
        if (contains(name, "sylphyo") || contains(name, "aodyo"))
            return { "aodyo-sylphyo", "Aodyo Sylphyo", 2, 11, 2, 0.9f, 0.25f };
        return { "generic-wind-controller", "通用电吹管", 2, 11, 2, 0.9f, 0.28f };
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

