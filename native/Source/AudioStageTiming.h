#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <cstddef>

namespace fengyin {
// Opt-in diagnostics only: fixed storage, no locks/allocations/logging in the
// callback. Each stage has one producer; the message thread consumes counters.
class AudioStageTiming {
public:
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    enum Stage { graph, instrumentEffects, accompaniment, master, callbackTail, callbackTotal, count };
    struct Result { uint64_t blocks=0, totalUs=0, maxUs=0, overBudget=0; };
    void record(Stage stage,double seconds,double budgetSeconds) noexcept {
        auto& value=values[stage];
        const auto us=static_cast<uint64_t>(seconds>0?seconds*1000000.0:0);
        value.blocks.fetch_add(1,std::memory_order_relaxed);
        value.total.fetch_add(us,std::memory_order_relaxed);
        auto previous=value.maximum.load(std::memory_order_relaxed);
        while(previous<us&&!value.maximum.compare_exchange_weak(previous,us,std::memory_order_relaxed)){}
        if(seconds>budgetSeconds)value.over.fetch_add(1,std::memory_order_relaxed);
    }
    std::array<Result,count> take() noexcept {
        std::array<Result,count> result{};
        for(std::size_t i=0;i<values.size();++i){auto& v=values[i];result[i]={v.blocks.exchange(0),v.total.exchange(0),v.maximum.exchange(0),v.over.exchange(0)};}
        return result;
    }
private:
    struct Values {std::atomic<uint64_t> blocks{0},total{0},maximum{0},over{0};};
    std::array<Values,count> values{};
};
}
