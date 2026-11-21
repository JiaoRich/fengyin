#pragma once

#include <cstdint>
#include <stdexcept>
#include <algorithm>
#include <atomic>
#include <mutex>
#include <optional>

namespace fengyin
{
inline constexpr int bleMidiConnectionTimeoutMs = 45000;
inline constexpr int bleMidiRequestTimeoutMs = 135000;
inline constexpr int bleMidiRecoveryTimeoutMs = 20000;

inline double bleMidiStageDeadline(double now, double requestDeadline, int budgetMs) noexcept
{
    return std::min(requestDeadline, now + budgetMs);
}

[[nodiscard]] constexpr bool isBleMidiAuthenticationError(std::uint8_t attError) noexcept
{
    // ATT Insufficient Authentication, Insufficient Encryption Key Size, Insufficient Encryption.
    return attError == 0x05 || attError == 0x0c || attError == 0x0f;
}

struct BleMidiAuthenticationRequired : std::runtime_error
{
    using std::runtime_error::runtime_error;
};
struct BleMidiRepairRequired : BleMidiAuthenticationRequired
{
    using BleMidiAuthenticationRequired::BleMidiAuthenticationRequired;
};

template<class OpenGatt, class IsPaired, class Close, class Pair>
void openBleMidiWithPairing(OpenGatt&& openGatt, IsPaired&& isPaired, Close&& close, Pair&& pair)
{
    try
    {
        openGatt();
        return;
    }
    catch (const BleMidiAuthenticationRequired& error)
    {
        if (isPaired())
        {
            close();
            try { openGatt(); return; }
            catch (const BleMidiAuthenticationRequired& again) { throw BleMidiRepairRequired(again.what()); }
        }
        pair(error);
    }
    // Each GATT/confirmation stage has its own bounded budget within the request.
    openGatt();
}

// Rare control events only. The normal MIDI callback only reads current().
// Posting rechecks the epoch while holding the same lock as reset(), so a callback
// which passed its early guard cannot later terminate a replacement connection.
class BleMidiLinkEvents
{
public:
    enum Cause : unsigned { disconnected=1, receiveError=2, statusError=4, sessionClosed=8 };
    struct Event { std::uint64_t epoch; unsigned causes; std::int32_t error; };
    std::uint64_t current() const noexcept { return epoch.load(); }
    void reset() { std::lock_guard lock(mutex); ++epoch; pending.reset(); }
    bool post(std::uint64_t expected, Cause cause, std::int32_t error=0)
    {
        std::lock_guard lock(mutex);
        if (expected != epoch.load()) return false;
        if (!pending) pending=Event{expected,0,0};
        pending->causes |= cause;
        if (error && !pending->error) pending->error=error;
        return true;
    }
    std::optional<Event> take()
    {
        std::lock_guard lock(mutex);
        auto result=pending; pending.reset(); return result;
    }
private:
    std::atomic<std::uint64_t> epoch{0};
    std::mutex mutex;
    std::optional<Event> pending;
};

class BleMidiRecoveryBudget
{
public:
    void newIntent() noexcept { remaining=2; stableSince=-1; }
    void connected(double now) noexcept { stableSince=now; }
    void lost(double now) noexcept
    {
        if (stableSince>=0 && now-stableSince>=30000) remaining=2;
        stableSince=-1;
    }
    bool beginAttempt() noexcept { if (!remaining) return false; --remaining; return true; }
    bool available() const noexcept { return remaining>0; }
private:
    int remaining=2;
    double stableSince=-1;
};
}
