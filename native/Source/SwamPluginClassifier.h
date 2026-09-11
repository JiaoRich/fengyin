#pragma once

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace fengyin
{
enum class SwamFamily { notSwam, saxophone, brass, woodwind, strings, other };

class SwamPluginClassifier
{
public:
    [[nodiscard]] static SwamFamily classify(std::string_view name,
                                             std::string_view manufacturer)
    {
        const auto text = lower(std::string(name) + " " + std::string(manufacturer));
        if (! contains(text, "swam") && ! contains(text, "audio modeling")
            && ! contains(text, "audiomodeling"))
            return SwamFamily::notSwam;
        if (contains(text, "sax"))
            return SwamFamily::saxophone;
        if (contains(text, "trumpet") || contains(text, "trombone")
            || contains(text, "horn") || contains(text, "tuba") || contains(text, "brass"))
            return SwamFamily::brass;
        if (contains(text, "flute") || contains(text, "clarinet")
            || contains(text, "oboe") || contains(text, "bassoon") || contains(text, "woodwind"))
            return SwamFamily::woodwind;
        if (contains(text, "violin") || contains(text, "viola")
            || contains(text, "cello") || contains(text, "double bass") || contains(text, "strings"))
            return SwamFamily::strings;
        return SwamFamily::other;
    }

private:
    static std::string lower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
                       [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    static bool contains(const std::string& text, std::string_view pattern)
    {
        return text.find(pattern) != std::string::npos;
    }
};
}

