#pragma once
#include <juce_core/juce_core.h>
#include <juce_cryptography/juce_cryptography.h>
#include <map>

namespace fengyin::sf2
{
inline constexpr juce::int64 maximumFileBytes = 256ll * 1024 * 1024;
inline juce::File resourceRoot()
{
   #if JUCE_WINDOWS
    return juce::File("C:\\fengyin\\SF2");
   #else
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("FengYin/SF2");
   #endif
}
inline bool validHash(const juce::String& hash)
{ return hash.length() == 64 && hash.containsOnly("0123456789abcdef"); }
inline juce::File resourceFile(const juce::String& hash, const juce::File& root = resourceRoot())
{ return validHash(hash) ? root.getChildFile(hash + ".sf2") : juce::File(); }

// Bounded RIFF walk before invoking the native parser. Only uncompressed SF2
// with the standard sample/table chunks is accepted in v1, never SF3/ROM/DLS.
inline juce::Result validateFile(const juce::File& file)
{
    auto input = file.createInputStream();
    const auto size = file.getSize();
    auto fail = [](const char* s) { return juce::Result::fail(juce::String::fromUTF8(s)); };
    if (!input || size < 24 || size > maximumFileBytes) return fail("SF2 文件无法读取、为空或超过 256 MB");
    const auto tag = [&]() { char bytes[4] {}; return input->read(bytes, 4) == 4 ? juce::String::fromUTF8(bytes, 4) : juce::String(); };
    if (tag() != "RIFF" || juce::uint64(input->readInt()) + 8 != juce::uint64(size) || tag() != "sfbk")
        return fail("不是完整的 SF2 音源文件");
    std::map<juce::String, juce::int64> tables;
    juce::int64 sampleBytes = 0;
    bool info = false, data = false, presets = false, version = false;
    int chunks = 0;
    while (input->getPosition() < size)
    {
        if (++chunks > 1024 || size - input->getPosition() < 8) return fail("SF2 目录长度异常");
        const auto kind = tag(); const auto count = juce::uint32(input->readInt());
        const auto end = input->getPosition() + count;
        if (end > size || kind != "LIST" || count < 4) return fail("SF2 区块不受支持或已损坏");
        const auto list = tag();
        if (list == "INFO") { if (info) return fail("SF2 信息重复"); info = true; }
        else if (list == "sdta") { if (data) return fail("SF2 采样重复"); data = true; }
        else if (list == "pdta") { if (presets) return fail("SF2 预设重复"); presets = true; }
        else return fail("SF2 目录类型不受支持");
        while (input->getPosition() < end)
        {
            if (++chunks > 1024 || end - input->getPosition() < 8) return fail("SF2 子区块长度异常");
            const auto child = tag(); const auto length = juce::uint32(input->readInt());
            const auto next = input->getPosition() + length + (length & 1);
            if (next > end) return fail("SF2 文件被截断");
            if (list == "pdta") {
                if (tables.contains(child)) return fail("SF2 预设表重复");
                tables[child] = length;
            } else if (list == "sdta" && child == "smpl") {
                if (sampleBytes || length < 92 || (length & 1)) return fail("SF2 采样数据异常");
                sampleBytes = length;
            } else if (list == "INFO" && child == "ifil") {
                if (version || length != 4 || input->readShort() != 2 || juce::uint16(input->readShort()) > 4)
                    return fail("只支持 SF2 2.x 格式，不支持压缩 SF3");
                version = true;
            }
            input->setPosition(next);
        }
        input->setPosition(end + (count & 1));
    }
    const std::pair<const char*, int> records[] { {"phdr",38},{"pbag",4},{"pmod",10},{"pgen",4},{"inst",22},{"ibag",4},{"imod",10},{"igen",4},{"shdr",46} };
    if (!version || !info || !data || !presets || !sampleBytes || tables.size() != 9) return fail("SF2 缺少采样或必要预设表");
    for (const auto& [name, stride] : records) {
        const auto n = tables[name];
        if (n < stride || n % stride || n / stride > 65536) return fail("SF2 预设表记录异常");
    }
    if (tables["phdr"] < 76 || tables["inst"] < 44 || tables["shdr"] < 92) return fail("SF2 没有可用音色");
    return juce::Result::ok();
}

// Called on a loader, never on the audio/message thread. Unique staging names
// prevent concurrent importers from leaving a partial final resource.
inline juce::Result importResource(const juce::File& source, juce::String& hash,
                                   const juce::File& root = resourceRoot())
{
    if (auto result = validateFile(source); result.failed()) return result;
    hash = juce::SHA256(source).toHexString();
    const auto target = resourceFile(hash, root);
    if (source == target) return juce::Result::ok();
    if (auto result = root.createDirectory(); result.failed()) return result;
    if (target.existsAsFile()) {
        if (juce::SHA256(target).toHexString() == hash) return juce::Result::ok();
        // A freshly selected, independently verified source can repair a
        // damaged hash-named managed file atomically. Other resources stay put.
    }
    if(root.getBytesFreeOnVolume()<source.getSize()+16ll*1024*1024)return juce::Result::fail(juce::String::fromUTF8("SF2 音源目录空间不足"));
    juce::TemporaryFile temporary(target);
    if (!source.copyFileTo(temporary.getFile()) || juce::SHA256(temporary.getFile()).toHexString() != hash)
        return juce::Result::fail(juce::String::fromUTF8("音源复制失败或空间不足"));
    if (!temporary.overwriteTargetFileWithTemporary()) return juce::Result::fail(juce::String::fromUTF8("无法写入 SF2 音源目录"));
    return juce::Result::ok();
}
}
