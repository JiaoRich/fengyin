#pragma once

namespace fengyin
{
// Device-worker state only. No clock reads, allocations, locks or I/O here.
// Stream health means successful output progress, never non-zero samples.
class AudioStreamRecovery
{
public:
    enum class Action { render, wait, retry, exhausted };
    Action poll(double nowMs, bool expectedRunning, bool healthy, bool recoverable) noexcept
    {
        if (! expectedRunning) { faultSince = stableSince = -1; return Action::wait; }
        if (healthy)
        {
            faultSince = -1;
            if (stableSince < 0) stableSince = nowMs;
            if (nowMs - stableSince >= 30000) attempts = 0;
            return Action::render;
        }
        stableSince = -1;
        if (faultSince < 0) faultSince = nowMs;
        if (! recoverable || attempts >= 3) return Action::exhausted;
        if (nowMs - faultSince < 1000 || nowMs < nextAttempt) return Action::wait;
        ++attempts;
        nextAttempt = nowMs + 1000.0 * (1 << (attempts - 1));
        return Action::retry;
    }
    void reset() noexcept { *this = {}; }
    int retryCount() const noexcept { return attempts; }
private:
    double faultSince = -1, stableSince = -1, nextAttempt = 0;
    int attempts = 0;
};
}
