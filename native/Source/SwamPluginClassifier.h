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

    [[nodiscard]] static const char* familyChineseName(SwamFamily family)
    {
        switch (family)
        {
            case SwamFamily::notSwam: return "其他音源";
            case SwamFamily::saxophone: return "萨克斯管";
            case SwamFamily::brass: return "铜管乐器";
            case SwamFamily::woodwind: return "木管乐器";
            case SwamFamily::strings: return "弦乐器";
            case SwamFamily::other: return "其他 SWAM 乐器";
        }
        return "其他音源";
    }

    [[nodiscard]] static const char* instrumentChineseName(std::string_view name)
    {
        const auto text = lower(std::string(name));
        if (contains(text, "soprano sax")) return "高音萨克斯";
        if (contains(text, "alto sax")) return "中音萨克斯";
        if (contains(text, "tenor sax")) return "次中音萨克斯";
        if (contains(text, "baritone sax")) return "上低音萨克斯";
        if (contains(text, "sax")) return "萨克斯";
        if (contains(text, "trumpet")) return "小号";
        if (contains(text, "trombone")) return "长号";
        if (contains(text, "french horn") || contains(text, "horn")) return "圆号";
        if (contains(text, "tuba")) return "大号";
        if (contains(text, "piccolo")) return "短笛";
        if (contains(text, "flute")) return "长笛";
        if (contains(text, "clarinet")) return "单簧管";
        if (contains(text, "oboe")) return "双簧管";
        if (contains(text, "bassoon")) return "巴松管";
        if (contains(text, "double bass")) return "低音提琴";
        if (contains(text, "violin")) return "小提琴";
        if (contains(text, "viola")) return "中提琴";
        if (contains(text, "cello")) return "大提琴";
        return "SWAM 乐器";
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
