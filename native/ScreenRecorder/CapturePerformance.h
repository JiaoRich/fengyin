#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <ostream>

// Opt-in only. All measurements/reporting run on the capture/encoder thread,
// never in the speaker capture or instrument audio callback.
class CapturePerformance {
public:
    enum Stage { readback, compose, audioWrite, videoWrite, finalize, count };
    explicit CapturePerformance(bool on):enabled(on){}
    struct Scope {
        CapturePerformance* owner;Stage stage;
        std::chrono::steady_clock::time_point start{};
        Scope(CapturePerformance* p,Stage s):owner(p&&p->enabled?p:nullptr),stage(s){if(owner)start=std::chrono::steady_clock::now();}
        ~Scope(){if(owner)owner->record(stage,std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now()-start).count());}
    };
    bool active() const noexcept{return enabled;}
    void write(std::ostream& out) const {
        const char* names[]{"gpuReadback","compose","audioWrite","videoWrite","finalize"};
        for(std::size_t i=0;i<values.size();++i){const auto& v=values[i];
            out<<names[i]<<" calls="<<v.calls<<" meanUs="<<(v.calls?double(v.total)/v.calls/1000:0)
               <<" maxUs="<<double(v.maximum)/1000<<" totalMs="<<double(v.total)/1000000<<"\n";
        }
    }
private:
    struct Value {uint64_t calls=0,total=0,maximum=0;};
    bool enabled;
    std::array<Value,count> values{};
    void record(Stage stage,int64_t nanos) noexcept {
        auto& v=values[stage];const auto n=uint64_t(nanos>0?nanos:0);
        ++v.calls;v.total+=n;if(v.maximum<n)v.maximum=n;
    }
};
