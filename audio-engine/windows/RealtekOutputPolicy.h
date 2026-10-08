#pragma once
#include <cstdint>
#include <vector>

namespace fengyin::audioengine
{
// Pure policy: endpoint display names never participate in pin selection.
struct AsioRouteNode
{
    long device, interfaceIndex, pin;
    std::uint32_t flags;
    bool realtek, output, available;
};
struct AsioRouteChange { AsioRouteNode node; std::uint32_t desired; };
inline bool planRealtekOutputs(const std::vector<AsioRouteNode>& nodes,
                              std::uint32_t enabled,
                              std::vector<AsioRouteChange>& plan)
{
    plan.clear();
    bool usable = false;
    for (const auto& n : nodes)
        usable |= n.pin >= 0 && n.realtek && n.output && n.available;
    if (!usable) return false; // Do not damage an existing route without a replacement.
    for (const auto& n : nodes)
    {
        bool on = false;
        if (n.pin >= 0) on = n.realtek && n.output && n.available;
        else
            for (const auto& p : nodes)
                on |= p.pin >= 0 && p.realtek && p.output && p.available
                    && p.device == n.device
                    && (n.interfaceIndex < 0 || p.interfaceIndex == n.interfaceIndex);
        const auto wanted = on ? n.flags | enabled : n.flags & ~enabled;
        if (wanted != n.flags) plan.push_back({n, wanted});
    }
    return true;
}

// Setter may fail after a partial write: include that operation in rollback.
// Readers and setters run only inside the driver's idle enumeration callback.
template<class Write, class Read>
bool applyRealtekPlan(const std::vector<AsioRouteChange>& plan,
                      Write write, Read read, bool& rollbackOK)
{
    rollbackOK = true;
    std::size_t attempted = 0;
    for (const auto& change : plan)
    {
        ++attempted;
        std::uint32_t actual = 0;
        if (write(change.node, change.desired) && read(change.node, actual)
            && (actual & 0x80000000u) == (change.desired & 0x80000000u)) continue;
        while (attempted > 0)
        {
            const auto& undo = plan[--attempted];
            std::uint32_t restored = 0;
            const bool written = write(undo.node, undo.node.flags);
            const bool readBack = read(undo.node, restored);
            rollbackOK = written && readBack
                && (restored & 0x80000000u) == (undo.node.flags & 0x80000000u) && rollbackOK;
        }
        return false;
    }
    return true;
}
}
