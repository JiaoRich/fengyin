#pragma once

#include <cstdint>
#include <stdexcept>

namespace fengyin
{
inline constexpr int bleMidiConnectionTimeoutMs = 45000;

[[nodiscard]] constexpr bool isBleMidiAuthenticationError(std::uint8_t attError) noexcept
{
    // ATT Insufficient Authentication, Insufficient Encryption Key Size, Insufficient Encryption.
    return attError == 0x05 || attError == 0x0c || attError == 0x0f;
}

struct BleMidiAuthenticationRequired : std::runtime_error
{
    using std::runtime_error::runtime_error;
};

template<class OpenGatt, class Pair>
void openBleMidiWithPairing(OpenGatt&& openGatt, Pair&& pair)
{
    try
    {
        openGatt();
        return;
    }
    catch (const BleMidiAuthenticationRequired& error)
    {
        pair(error);
    }
    // Retry once under the same request deadline after Windows pairs the device.
    openGatt();
}
}
