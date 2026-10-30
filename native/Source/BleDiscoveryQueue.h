#pragma once
#include <algorithm>
#include <cstdint>
#include <deque>
#include <iterator>
#include <optional>
#include <string>

namespace fengyin {
// Control thread/callback mutex owns this queue. Never used on the audio thread.
// A scan response upgrades its original advertisement instead of losing to it.
struct BleAdvert {
    uint64_t address=0;
    int addressType=0;
    std::string name;
    bool midi=false;
    double receivedMs=0;
};
class BleDiscoveryQueue {
public:
    static constexpr size_t capacity=128;
    void clear() { pending.clear(); received=merged=dropped=0; maxWaitMs=0; }
    void offer(BleAdvert value) {
        ++received;
        auto same=std::find_if(pending.begin(),pending.end(),[&](const auto& p){return p.address==value.address&&p.addressType==value.addressType;});
        if(same!=pending.end()) {
            if(!value.name.empty())same->name=std::move(value.name);
            same->midi|=value.midi; // retain positive service evidence
            ++merged;return; // retain first receipt time to measure queue delay
        }
        if(pending.size()==capacity) {
            // A verified UUID must not be crowded out by unrelated advertisers.
            auto weak=std::find_if(pending.rbegin(),pending.rend(),[](const auto& p){return !p.midi;});
            if(!value.midi||weak==pending.rend()){++dropped;return;}
            pending.erase(std::next(weak).base());++dropped;
        }
        pending.push_back(std::move(value));
    }
    std::optional<BleAdvert> take(double nowMs) {
        if(pending.empty())return {};
        auto next=std::find_if(pending.begin(),pending.end(),[](const auto& p){return p.midi;});
        if(next==pending.end())next=pending.begin();
        auto value=std::move(*next);pending.erase(next);
        maxWaitMs=std::max(maxWaitMs,std::max(0.0,nowMs-value.receivedMs));return value;
    }
    bool empty() const {return pending.empty();}
    size_t size() const {return pending.size();}
    uint64_t received=0,merged=0,dropped=0;
    double maxWaitMs=0;
private:
    std::deque<BleAdvert> pending;
};
}
