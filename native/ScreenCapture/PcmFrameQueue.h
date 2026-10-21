#pragma once
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace fengyin::screen {
// One WASAPI producer, one disk consumer. Capacity is in stereo PCM frames,
// independent of endpoint packet size. No allocation or disk access in push.
class PcmFrameQueue {
    const std::size_t capacity;
    std::vector<std::int16_t> samples;
    alignas(64) std::atomic<std::uint64_t> written{0};
    alignas(64) std::atomic<std::uint64_t> read{0};
    std::atomic<std::uint64_t> peak{0};
public:
    explicit PcmFrameQueue(std::size_t frames=48000*10):capacity(frames),samples(frames*2){
        if(!frames)throw std::invalid_argument("Empty PCM queue");
    }
    bool push(const std::int16_t* source,std::size_t frames,bool silence=false){
        const auto w=written.load(std::memory_order_relaxed),r=read.load(std::memory_order_acquire);
        if(frames>capacity-(w-r))return false;
        const auto offset=std::size_t(w%capacity),first=std::min(frames,capacity-offset);
        const auto copy=[&](std::size_t dst,std::size_t src,std::size_t count){
            if(silence)std::memset(samples.data()+dst*2,0,count*4);
            else if(count)std::memcpy(samples.data()+dst*2,source+src*2,count*4);
        };
        copy(offset,0,first);copy(0,first,frames-first);
        const auto used=w-r+frames;
        if(used>peak.load(std::memory_order_relaxed))peak.store(used,std::memory_order_relaxed);
        written.store(w+frames,std::memory_order_release);return true;
    }
    std::size_t pop(std::int16_t* target,std::size_t maxFrames){
        const auto r=read.load(std::memory_order_relaxed),w=written.load(std::memory_order_acquire);
        const auto frames=std::min<std::uint64_t>(w-r,maxFrames);
        const auto offset=std::size_t(r%capacity),first=std::min<std::size_t>(frames,capacity-offset);
        if(first)std::memcpy(target,samples.data()+offset*2,first*4);
        if(frames>first)std::memcpy(target+first*2,samples.data(),(frames-first)*4);
        // Return space before disk IO. The writer owns its separate batch.
        read.store(r+frames,std::memory_order_release);return std::size_t(frames);
    }
    std::uint64_t available()const{
        const auto r=read.load(std::memory_order_relaxed);
        return written.load(std::memory_order_acquire)-r;
    }
    std::uint64_t highWaterFrames()const{return peak.load(std::memory_order_relaxed);}
};
}
