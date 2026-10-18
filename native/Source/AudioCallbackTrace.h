#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>

namespace fengyin::audioTrace {
// Diagnostic wall time, not CPU time. A plug-in's own internal waits remain
// inside pluginCall; they cannot be identified without its source/profiler.
enum class Event : unsigned { playerLock, graphLock, nodeLock, nodeBody,
                              vstLock, vstAdapter, pluginCall, techniques, midiDrain, count };
inline constexpr unsigned eventCount=static_cast<unsigned>(Event::count);
struct Sink {
    void* context=nullptr;
    void (*record)(void*,Event,const void*,uint64_t)=nullptr;
    void (*begin)(void*,Event,const void*)=nullptr; // optional test observer
};
inline thread_local Sink* current=nullptr;
inline uint64_t now() noexcept {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}
class Session {
public:
    explicit Session(Sink* sink) noexcept:previous(current){current=sink;}
    ~Session(){current=previous;}
    Session(const Session&)=delete;
    Session& operator=(const Session&)=delete;
private:
    Sink* previous;
};
class Scope {
public:
    Scope(Event kind,const void* object=nullptr) noexcept
        :sink(current),event(kind),owner(object),started(sink?now():0){
        if(sink&&sink->begin)sink->begin(sink->context,event,owner);
    }
    ~Scope(){finish();}
    void finish() noexcept {
        if(auto* target=sink){sink=nullptr;target->record(target->context,event,owner,now()-started);}
    }
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
private:
    Sink* sink;
    Event event;
    const void* owner;
    uint64_t started;
};
class Accumulator {
public:
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    // Role 0 = host/unknown; role 1 = instrument; role 2 = serial effects.
    static constexpr unsigned roles=3;
    struct Result {uint64_t calls=0,totalNs=0,maxNs=0;};
    std::atomic<const void*> instrument{nullptr};
    Sink sink() noexcept {return {this,[](void* context,Event event,const void* owner,uint64_t ns){
        static_cast<Accumulator*>(context)->record(event,owner,ns);
    }};}
    void record(Event event,const void* owner,uint64_t ns) noexcept {
        const unsigned role=owner?(owner==instrument.load(std::memory_order_relaxed)?1u:2u):0u;
        auto& value=values[static_cast<unsigned>(event)*roles+role];
        value.calls.fetch_add(1,std::memory_order_relaxed);
        value.total.fetch_add(ns,std::memory_order_relaxed);
        auto maximum=value.maximum.load(std::memory_order_relaxed);
        while(maximum<ns&&!value.maximum.compare_exchange_weak(maximum,ns,std::memory_order_relaxed)){}
    }
    std::array<Result,eventCount*roles> take() noexcept {
        std::array<Result,eventCount*roles> result{};
        for(unsigned i=0;i<result.size();++i){auto& value=values[i];
            result[i]={value.calls.exchange(0),value.total.exchange(0),value.maximum.exchange(0)};
        }
        return result;
    }
private:
    struct Value{std::atomic<uint64_t> calls{0},total{0},maximum{0};};
    std::array<Value,eventCount*roles> values{};
};
class Cadence {
public:
    struct Result{uint64_t calls=0,minFrames=0,maxFrames=0,maxGapNs=0;};
    void restart() noexcept {lastStart=0;epoch.fetch_add(1,std::memory_order_relaxed);}
    void record(uint64_t start,int frames) noexcept {
        calls.fetch_add(1,std::memory_order_relaxed);
        const auto n=static_cast<uint64_t>(frames>0?frames:0);
        auto low=minFrames.load(std::memory_order_relaxed);
        while(n<low&&!minFrames.compare_exchange_weak(low,n,std::memory_order_relaxed)){}
        maximum(maxFrames,n);
        if(lastStart&&start>=lastStart)maximum(maxGap,start-lastStart);
        lastStart=start;
    }
    Result take() noexcept {
        const auto low=minFrames.exchange(UINT64_MAX);
        return {calls.exchange(0),low==UINT64_MAX?0:low,maxFrames.exchange(0),maxGap.exchange(0)};
    }
    std::atomic<uint64_t> epoch{0};
private:
    static void maximum(std::atomic<uint64_t>& target,uint64_t n) noexcept {
        auto old=target.load(std::memory_order_relaxed);
        while(old<n&&!target.compare_exchange_weak(old,n,std::memory_order_relaxed)){}
    }
    uint64_t lastStart=0; // audio callback, or device restart while it is stopped
    std::atomic<uint64_t> calls{0},minFrames{UINT64_MAX},maxFrames{0},maxGap{0};
};
}
