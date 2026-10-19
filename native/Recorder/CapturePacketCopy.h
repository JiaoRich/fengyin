#pragma once
#include <cstddef>
#include <cstring>

namespace fengyin::capture {
// WASAPI marks a complete packet silent independently of its data pointer.
// Only write to our own buffer; the endpoint buffer belongs to Windows.
inline bool copyCapturePacket(void* destination, std::size_t capacity,
                              const void* source, std::size_t bytes, bool silent) noexcept {
    if (bytes > capacity || (bytes != 0 && destination == nullptr)) return false;
    if (bytes == 0) return true;
    if (silent) std::memset(destination, 0, bytes);
    else {
        if (source == nullptr) return false;
        std::memcpy(destination, source, bytes);
    }
    return true;
}
}
