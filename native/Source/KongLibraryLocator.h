#pragma once
#include <juce_core/juce_core.h>

namespace fengyin
{
// Inventory only: never reads/decrypts KAI contents or treats filenames as plugin IDs.
class KongLibraryLocator
{
public:
    struct InstrumentFile
    {
        juce::String key;
        juce::String chineseName;
        juce::String originalName;
        bool recognised = false;
    };

    struct Result
    {
        juce::File directory;
        juce::String source;
        juce::StringArray files;
        bool exists = false;
        bool truncated = false;
        bool ready() const { return exists && ! files.isEmpty(); }
    };

    static Result inspect(const juce::File& directory, const juce::String& source)
    {
        Result result;
        result.directory = directory;
        result.source = source;
        result.exists = directory.isDirectory();
        if (! result.exists) return result;
        int visited = 0;
        for (const auto& entry : juce::RangedDirectoryIterator(directory, false, "*", juce::File::findFiles))
        {
            if (++visited > 4096) { result.truncated = true; break; }
            if (entry.getFile().hasFileExtension("kai")) result.files.add(entry.getFile().getFileName());
        }
        result.files.sort(true);
        return result;
    }

    static juce::String readPlainConfigPath(const juce::File& file)
    {
        if (! file.existsAsFile() || file.getSize() > 1024 * 1024) return {};
        // Supported plaintext variants only; never attempt to decode the proprietary config.
        if (auto xml = juce::XmlDocument::parse(file))
        {
            const auto path = xml->getStringAttribute("KAIFolderPath");
            if (path.isNotEmpty()) return path;
            for (const auto* child : xml->getChildIterator())
                if (child->getStringAttribute("name") == "KAIFolderPath")
                    return child->getStringAttribute("val", child->getStringAttribute("value"));
        }
        return {};
    }

    static InstrumentFile describe(const juce::String& filename)
    {
        const auto stem = juce::File(filename).getFileNameWithoutExtension();
        const auto normal = stem.toLowerCase().removeCharacters(" _-().");
        struct Name { const char* token; const char* key; const char* chinese; };
        static constexpr Name names[] {
            {"bianzhongpro","kong-bianzhong-pro","专业编钟"}, {"guzhengii","kong-guzheng-2","古筝二"},
            {"erhuii","kong-erhu-2","二胡二"}, {"erhu","kong-erhu","二胡"},
            {"banhu","kong-banhu","板胡"}, {"bawu","kong-bawu","巴乌"},
            {"bianqing","kong-bianqing","编磬"}, {"bianzhong","kong-bianzhong","编钟"},
            {"gaohu","kong-gaohu","高胡"}, {"guanzi","kong-guanzi","管子"},
            {"guqin","kong-guqin","古琴"}, {"guzheng","kong-guzheng","古筝"},
            {"hulusi","kong-hulusi","葫芦丝"}, {"jinghu","kong-jinghu","京胡"},
            {"liuqin","kong-liuqin","柳琴"}, {"matouqin","kong-matouqin","马头琴"},
            {"nanxiao","kong-nanxiao","南箫"}, {"pipa","kong-pipa","琵琶"},
            {"sanxian","kong-sanxian","三弦"}, {"sheng","kong-sheng","笙"},
            {"suona","kong-suona","唢呐"}, {"dizi","kong-dizi","笛子"},
            {"xun","kong-xun","埙"}, {"yangqin","kong-yangqin","扬琴"},
            {"zhonghu","kong-zhonghu","中胡"}, {"ruan","kong-ruan","阮"},
            {"kekejiaoxiangtongguan","kong-kirk-brass","柯克交响·铜管"},
            {"kekejiaoxiangdaji","kong-kirk-percussion","柯克交响·打击乐"},
            {"kekejiaoxiangtiqin","kong-kirk-strings","柯克交响·提琴"},
            {"kekejiaoxiangmuguan","kong-kirk-woodwind","柯克交响·木管"},
            {"dajiyue","kong-percussion","打击乐"},
            {"kirkbrass","kong-kirk-brass","柯克交响·铜管"},
            {"kirkpercussion","kong-kirk-percussion","柯克交响·打击乐"},
            {"kirkstrings","kong-kirk-strings","柯克交响·提琴"},
            {"kirkwoodwind","kong-kirk-woodwind","柯克交响·木管"},
            {"percussion","kong-percussion","打击乐"}
        };
        for (const auto& name : names)
        {
            const auto token = juce::String(name.token);
            if (normal == token)
                return { name.key, juce::String::fromUTF8(name.chinese), filename, true };
            const auto suffix = normal.substring(token.length());
            if (normal.startsWith(token) && suffix.isNotEmpty() && suffix.containsOnly("0123456789"))
                return { juce::String(name.key) + "-" + suffix,
                         juce::String::fromUTF8(name.chinese) + suffix, filename, true };
        }
        auto display = stem.replaceCharacters("_-", "  ").trim();
        while (display.contains("  ")) display = display.replace("  ", " ");
        return { "kong-kai-" + juce::String::toHexString(stem.hashCode64()), display, filename, false };
    }

    static Result resolve(const juce::File& savedChoice, const juce::File& config,
                          const juce::File& defaultDirectory)
    {
        // A readable live engine setting takes precedence over an older manual choice.
        const auto configured = readPlainConfigPath(config).trim();
        if (juce::File::isAbsolutePath(configured)) return inspect(juce::File(configured), "engine");
        if (savedChoice.existsAsFile() && savedChoice.getSize() <= 32768)
        {
            const auto saved = savedChoice.loadFileAsString().trim();
            if (juce::File::isAbsolutePath(saved)) return inspect(juce::File(saved), "selected");
        }
        const auto fallback = inspect(defaultDirectory, "default");
        return fallback.ready() ? fallback : Result{};
    }
};
}
