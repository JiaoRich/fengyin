#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

namespace fengyin
{
// A bounded, allocation-free observer. MIDI channel is part of controller identity.
// Pair numbering alone is NOT evidence: require repeated adjacent coarse/fine
// updates. Parallel 7-bit aliases require matching values at changing levels.
class BreathControllerFamily
{
public:
    struct Result { bool breath = false; float value = 0; };
    void reset(int selected) noexcept { *this = {}; primary = std::clamp(selected, 0, 127); }
    Result observe(int channel, int cc, int value, double seconds) noexcept
    {
        if (channel < 1 || channel > 16 || cc < 0 || cc > 127) return {};
        auto& s = channels[static_cast<size_t>(channel - 1)];
        value = std::clamp(value, 0, 127);
        auto& event = s.events[static_cast<size_t>(cc)];
        event = { value, seconds, true, ++s.sequence };
        const auto close = [seconds](const Event& e) { return e.seen && std::abs(seconds-e.time) <= .010; };
        if (cc == primary)
        {
            for (int n = 0; n < 128; ++n)
            {
                if (n == primary || n == primary + 32) continue;
                const auto& other = s.events[static_cast<size_t>(n)];
                if (close(other)) observeAlias(s,n);
            }
        }
        else if (close(s.events[static_cast<size_t>(primary)]) && cc != primary + 32)
            observeAlias(s,cc);
        if (primary < 32 && (cc == primary || cc == primary + 32))
        {
            const auto& coarse = s.events[static_cast<size_t>(primary)];
            const auto& fine = s.events[static_cast<size_t>(primary+32)];
            if (close(coarse) && close(fine) && (coarse.sequence > s.pairSequence && fine.sequence > s.pairSequence))
            {
                s.pairSequence = std::max(coarse.sequence, fine.sequence);
                if (s.pairCount < 4) ++s.pairCount;
            }
            if (s.pairCount >= 4 && coarse.seen)
            {
                // Never reuse a stale fine byte after a pause / a plain 7-bit switch.
                const int low = close(fine) ? fine.value : 0;
                const auto raw = ((coarse.value << 7) | low) / 16383.0f;
                return {true, coarse.value == 0 && !close(fine) ? 0.f : raw};
            }
        }
        if (cc == primary) return {true, value / 127.0f};
        return {};
    }
    bool protectedController(int channel, int cc) const noexcept
    {
        if (cc == primary) return true;
        if (channel < 1 || channel > 16 || cc < 0 || cc > 127) return false;
        const auto& s = channels[static_cast<size_t>(channel-1)];
        if (primary < 32 && cc == primary+32 && s.pairCount >= 2) return true;
        return s.aliasCount[static_cast<size_t>(cc)] >= 3;
    }
    bool probableBreath(int channel, int cc, double now) const noexcept
    {
        if (protectedController(channel,cc)) return true;
        if (channel < 1 || channel > 16 || cc < 0 || cc > 127) return false;
        const auto& s = channels[static_cast<size_t>(channel-1)];
        const auto& main = s.events[static_cast<size_t>(primary)];
        const auto& candidate = s.events[static_cast<size_t>(cc)];
        if (!main.seen || !candidate.seen || std::abs(main.time-candidate.time)>.010 || now-main.time>.05) return false;
        return (primary < 32 && cc == primary+32) || main.value == candidate.value;
    }
private:
    struct Event { int value=0; double time=0; bool seen=false; uint64_t sequence=0; };
    struct Channel
    {
        std::array<Event,128> events{};
        std::array<uint8_t,128> aliasCount{};
        std::array<int,128> aliasLast{};
        std::array<uint64_t,128> aliasSequence{};
        int pairCount=0; uint64_t sequence=0, pairSequence=0;
    };
    void observeAlias(Channel& s,int cc) noexcept
    {
        auto i=static_cast<size_t>(cc);
        const auto& a=s.events[static_cast<size_t>(primary)];const auto& b=s.events[i];
        // Each observation consumes a fresh pair, irrespective of arrival order.
        if(a.sequence<=s.aliasSequence[i] || b.sequence<=s.aliasSequence[i])return;
        s.aliasSequence[i]=std::max(a.sequence,b.sequence);
        if(a.value!=b.value){s.aliasCount[i]=0;s.aliasLast[i]=-1;return;}
        const int value=a.value;
        if (value != s.aliasLast[i]) {s.aliasLast[i]=value; if(s.aliasCount[i]<3)++s.aliasCount[i];}
    }
    int primary=2;
    std::array<Channel,16> channels{};
};
}
