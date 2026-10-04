#include "FengYinAudioRing.h"

namespace
{
constexpr ULONG capacity = 1u << 18; // 262,144 bytes; power-of-two wrapping.
alignas(64) UCHAR storage[capacity] {};
KSPIN_LOCK lock {};
ULONGLONG writePosition = 0;
ULONGLONG readPosition = 0;

void copyIntoRing(ULONGLONG position, const UCHAR* source, ULONG bytes) noexcept
{
    const auto offset = static_cast<ULONG>(position & (capacity - 1));
    const auto first = bytes < capacity - offset ? bytes : capacity - offset;
    RtlCopyMemory(storage + offset, source, first);
    if (bytes > first) RtlCopyMemory(storage, source + first, bytes - first);
}

void copyFromRing(ULONGLONG position, UCHAR* destination, ULONG bytes) noexcept
{
    const auto offset = static_cast<ULONG>(position & (capacity - 1));
    const auto first = bytes < capacity - offset ? bytes : capacity - offset;
    RtlCopyMemory(destination, storage + offset, first);
    if (bytes > first) RtlCopyMemory(destination + first, storage, bytes - first);
}
}

void FengYinAudioRingInitialize() noexcept
{
    KeInitializeSpinLock(&lock);
    writePosition = 0;
    readPosition = 0;
    RtlZeroMemory(storage, sizeof(storage));
}

void FengYinAudioRingResetReader() noexcept
{
    KIRQL previousIrql;
    KeAcquireSpinLock(&lock, &previousIrql);
    readPosition = writePosition;
    KeReleaseSpinLock(&lock, previousIrql);
}

void FengYinAudioRingWrite(const UCHAR* source, ULONG byteCount) noexcept
{
    if (source == nullptr || byteCount == 0) return;
    if (byteCount > capacity)
    {
        source += byteCount - capacity;
        byteCount = capacity;
    }
    KIRQL previousIrql;
    KeAcquireSpinLock(&lock, &previousIrql);
    const auto nextWrite = writePosition + byteCount;
    if (nextWrite - readPosition > capacity) readPosition = nextWrite - capacity;
    copyIntoRing(writePosition, source, byteCount);
    writePosition = nextWrite;
    KeReleaseSpinLock(&lock, previousIrql);
}

void FengYinAudioRingRead(UCHAR* destination, ULONG byteCount) noexcept
{
    if (destination == nullptr || byteCount == 0) return;
    KIRQL previousIrql;
    KeAcquireSpinLock(&lock, &previousIrql);
    const auto available64 = writePosition - readPosition;
    const auto available = static_cast<ULONG>(available64 < byteCount ? available64 : byteCount);
    if (available != 0)
    {
        copyFromRing(readPosition, destination, available);
        readPosition += available;
    }
    KeReleaseSpinLock(&lock, previousIrql);
    if (available < byteCount) RtlZeroMemory(destination + available, byteCount - available);
}
