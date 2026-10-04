#pragma once

#if defined(_WIN32)
#include <mmdeviceapi.h>
#include <wrl/client.h>
#endif

#include <string>

namespace fengyin::audioengine
{
#if defined(_WIN32)
// Resolve a real hardware endpoint without selecting FengYin's own virtual
// speaker. This prevents feedback after the virtual speaker becomes default.
bool selectPhysicalOutput(IMMDeviceEnumerator& enumerator,
                          const std::wstring& preferredEndpointId,
                          Microsoft::WRL::ComPtr<IMMDevice>& selected,
                          std::wstring& selectedId,
                          std::wstring& selectedName,
                          std::wstring& error) noexcept;
#endif
}
