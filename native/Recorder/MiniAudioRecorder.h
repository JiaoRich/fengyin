#pragma once
#include <filesystem>
#include <memory>
#include <string>

// Third-party WASAPI capture / resampling / PCM WAV encoding. This helper is
// separate from FengYin's instrument graph and never changes its buffer/route.
class MiniAudioRecorder {
public:
    MiniAudioRecorder();
    ~MiniAudioRecorder();
    void start(const std::filesystem::path& output,const std::wstring& endpoint);
    void drain();
    void finish();
    static void selfTest(const std::filesystem::path& output);
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
