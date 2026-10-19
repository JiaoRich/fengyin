#pragma once
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace fengyin
{
// LibVLC owns both audio and video. No decoding, file IO, or VLC calls run on
// the instrument callback or the UI thread. Commands are a bounded mailbox.
class VlcPlaybackEngine
{
public:
    struct State
    {
        int generation = 0, seekRevision = 0;
        bool ready = false, playing = false, finished = false;
        double position = 0, duration = 0;
        int decodedVideo = 0, displayedVideo = 0, lostVideo = 0, decodedAudio = 0;
        std::string error;
    };
    explicit VlcPlaybackEngine(std::filesystem::path runtimeDirectory, void* videoWindow);
    ~VlcPlaybackEngine();
    void load(std::string utf8Path, int generation);
    void setPlaying(bool);
    void seek(double seconds, int revision);
    void setVolume(float);
    void unload();
    State getState() const;
    // The UI must keep pumping Win32 messages while VLC releases its renderer.
    void requestShutdown();
    bool hasStopped() const noexcept { return stopped.load(); }
#if defined(FENGYIN_VLC_TEST_PCM)
    std::atomic<uint64_t> testPcmSamples{0},testPcmNonzero{0};
    std::atomic<int> testPlaybackState{-1};
#endif
private:
    struct Request
    {
        std::string path;
        int generation = 0, playRevision = 0, seekRevision = 0;
        bool playing = false, quitting = false;
        double position = 0;
        float volume = .72f;
    };
    void run();
    std::filesystem::path directory;
    void* window;
    mutable std::mutex mutex;
    std::condition_variable wake;
    Request request;
    State state;
    std::atomic<bool> stopped {false};
    std::thread worker;
};
}
