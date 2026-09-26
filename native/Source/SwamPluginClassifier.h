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
        if (contains(text, "flute") || contains(text, "clarinet")
            || contains(text, "oboe") || contains(text, "bassoon")
            || contains(text, "english horn") || contains(text, "cor anglais")
            || contains(text, "woodwind"))
            return SwamFamily::woodwind;
        if (contains(text, "trumpet") || contains(text, "trombone")
            || contains(text, "horn") || contains(text, "tuba") || contains(text, "euphonium")
            || contains(text, "flugelhorn") || contains(text, "brass"))
            return SwamFamily::brass;
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

    [[nodiscard]] static const char* familyKey(SwamFamily family)
    {
        switch (family)
        {
            case SwamFamily::saxophone: return "saxophone";
            case SwamFamily::brass: return "brass";
            case SwamFamily::woodwind: return "woodwind";
            case SwamFamily::strings: return "strings";
            case SwamFamily::other: return "swam-other";
            case SwamFamily::notSwam: return "other";
        }
        return "other";
    }

    [[nodiscard]] static const char* instrumentChineseName(std::string_view name)
    {
        const auto text = lower(std::string(name));
        if (contains(text, "soprano sax")) return "高音萨克斯";
        if (contains(text, "alto sax")) return "中音萨克斯";
        if (contains(text, "tenor sax")) return "次中音萨克斯";
        if (contains(text, "baritone sax")) return "上低音萨克斯";
        if (contains(text, "sax")) return "萨克斯";
        if (contains(text, "flugelhorn") && contains(text, "eb")) return "降E调柔音号";
        if (contains(text, "flugelhorn")) return "柔音号";
        if (contains(text, "piccolo trumpet")) return "高音小号";
        if (contains(text, "trumpet") && contains(text, "(c)")) return "C调小号";
        if (contains(text, "trumpet c")) return "C调小号";
        if (contains(text, "trumpet")) return "小号";
        if (contains(text, "double bass trombone")) return "倍低音长号";
        if (contains(text, "tenor bass trombone")) return "次中低音长号";
        if (contains(text, "bass trombone")) return "低音长号";
        if (contains(text, "alto trombone")) return "中音长号";
        if (contains(text, "trombone")) return "次中音长号";
        if (contains(text, "euphonium")) return "上低音号";
        if (contains(text, "bass tuba")) return "低音大号";
        if (contains(text, "tuba") && contains(text, "eb")) return "降E调大号";
        if (contains(text, "tuba")) return "大号";
        if (contains(text, "french horn") && contains(text, "bb")) return "降B调圆号";
        if (contains(text, "french horn") || contains(text, "horn")) return "F调圆号";
        if (contains(text, "piccolo")) return "短笛";
        if (contains(text, "bass flute")) return "低音长笛";
        if (contains(text, "alto flute")) return "中音长笛";
        if (contains(text, "flute")) return "长笛";
        if (contains(text, "bass clarinet")) return "低音单簧管";
        if (contains(text, "clarinet")) return "单簧管";
        if (contains(text, "english horn") || contains(text, "cor anglais")) return "英国管";
        if (contains(text, "oboe")) return "双簧管";
        if (contains(text, "contrabassoon")) return "倍低音巴松管";
        if (contains(text, "bassoon")) return "巴松管";
        const auto section = contains(text, "section") || contains(text, "ensemble");
        if (contains(text, "double bass")) return section ? "低音提琴重奏" : "低音提琴独奏";
        if (contains(text, "violin")) return section ? "小提琴重奏" : "小提琴独奏";
        if (contains(text, "viola")) return section ? "中提琴重奏" : "中提琴独奏";
        if (contains(text, "cello")) return section ? "大提琴重奏" : "大提琴独奏";
        return "SWAM 乐器";
    }

    // Stable UI key used by the web front-end to select the matching instrument artwork.
    [[nodiscard]] static const char* instrumentKey(std::string_view name)
    {
        const auto text = lower(std::string(name));
        if (contains(text, "soprano sax")) return "soprano-sax";
        if (contains(text, "alto sax")) return "alto-sax";
        if (contains(text, "tenor sax")) return "tenor-sax";
        if (contains(text, "baritone sax")) return "baritone-sax";
        if (contains(text, "flugelhorn") && contains(text, "eb")) return "flugelhorn-eb";
        if (contains(text, "flugelhorn")) return "flugelhorn";
        if (contains(text, "piccolo trumpet")) return "piccolo-trumpet";
        if (contains(text, "trumpet") && (contains(text, "(c)") || contains(text, "trumpet c"))) return "trumpet-c";
        if (contains(text, "trumpet")) return "trumpet";
        if (contains(text, "double bass trombone")) return "double-bass-trombone";
        if (contains(text, "tenor bass trombone")) return "tenor-bass-trombone";
        if (contains(text, "bass trombone")) return "bass-trombone";
        if (contains(text, "alto trombone")) return "alto-trombone";
        if (contains(text, "trombone")) return "tenor-trombone";
        if (contains(text, "bass tuba")) return "bass-tuba";
        if (contains(text, "tuba") && contains(text, "eb")) return "tuba-eb";
        if (contains(text, "euphonium")) return "euphonium";
        if (contains(text, "french horn") && contains(text, "bb")) return "horn-bb";
        if (contains(text, "french horn") || contains(text, "horn")) return "horn-f";
        if (contains(text, "piccolo")) return "piccolo";
        if (contains(text, "bass flute")) return "bass-flute";
        if (contains(text, "alto flute")) return "alto-flute";
        if (contains(text, "flute")) return "flute";
        if (contains(text, "bass clarinet")) return "bass-clarinet";
        if (contains(text, "clarinet")) return "clarinet";
        if (contains(text, "english horn") || contains(text, "cor anglais")) return "english-horn";
        if (contains(text, "oboe")) return "oboe";
        if (contains(text, "contrabassoon")) return "contrabassoon";
        if (contains(text, "bassoon")) return "bassoon";
        const auto section = contains(text, "section") || contains(text, "ensemble");
        if (contains(text, "double bass")) return section ? "double-bass-section" : "double-bass";
        if (contains(text, "violin")) return section ? "violin-section" : "violin";
        if (contains(text, "viola")) return section ? "viola-section" : "viola";
        if (contains(text, "cello")) return section ? "cello-section" : "cello";
        return "alto-sax";
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
