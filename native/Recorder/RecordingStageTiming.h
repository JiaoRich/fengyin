#pragma once
#include <atomic>
#include <cstdint>

namespace fengyin::capture {
// SRL calls audio inspection before texture processing/RenderFrame and its
// frame callback after RenderFrame. Fixed counters reveal where progress stops
// without tracing or writing a file on either of those library callbacks.
class RecordingStageTiming {
public:
    struct Snapshot {std::uint64_t audioPackets,paddedBytes,postAudioToFrameMaxMs,frameIntervalMaxMs,audioGapMs,frameGapMs;};
    void audio(std::uint64_t now,std::uint64_t padding) noexcept {
        lastAudio.store(now,std::memory_order_relaxed);
        audioPackets.fetch_add(1,std::memory_order_relaxed);
        paddedBytes.fetch_add(padding,std::memory_order_relaxed);
    }
    void frame(std::uint64_t now) noexcept {
        const auto previous=lastFrame.exchange(now,std::memory_order_relaxed);
        const auto audioAt=lastAudio.load(std::memory_order_relaxed);
        if(previous && now>=previous)maximum(frameInterval,now-previous);
        if(audioAt && now>=audioAt)maximum(postAudio,now-audioAt);
    }
    Snapshot take(std::uint64_t now) noexcept {
        const auto audioAt=lastAudio.load(std::memory_order_relaxed),frameAt=lastFrame.load(std::memory_order_relaxed);
        return {audioPackets.exchange(0),paddedBytes.exchange(0),postAudio.exchange(0),frameInterval.exchange(0),
            audioAt && now>=audioAt?now-audioAt:0,frameAt && now>=frameAt?now-frameAt:0};
    }
private:
    static void maximum(std::atomic<std::uint64_t>& value,std::uint64_t next) noexcept {
        auto old=value.load(std::memory_order_relaxed);
        while(old<next && !value.compare_exchange_weak(old,next,std::memory_order_relaxed)){}
    }
    std::atomic<std::uint64_t> lastAudio{0},lastFrame{0},audioPackets{0},paddedBytes{0},postAudio{0},frameInterval{0};
};
}
