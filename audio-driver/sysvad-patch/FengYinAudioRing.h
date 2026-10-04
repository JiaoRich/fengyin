/*
  FengYin system-audio cable for Microsoft's SysVAD sample architecture.
  The surrounding SysVAD source remains under the Microsoft Public License.
*/
#pragma once

#include <ntddk.h>

void FengYinAudioRingInitialize() noexcept;
void FengYinAudioRingResetReader() noexcept;
void FengYinAudioRingWrite(_In_reads_bytes_(byteCount) const BYTE* source,
                           _In_ ULONG byteCount) noexcept;
void FengYinAudioRingRead(_Out_writes_bytes_(byteCount) BYTE* destination,
                          _In_ ULONG byteCount) noexcept;
