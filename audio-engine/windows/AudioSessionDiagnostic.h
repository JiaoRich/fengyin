#pragma once

#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

// Read-only snapshot. Never changes endpoint settings or stops another app.
inline void logAudioSessions()
{
    using Microsoft::WRL::ComPtr;
    ComPtr<IMMDeviceEnumerator> enumerator;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                IID_PPV_ARGS(&enumerator)))) return;
    ComPtr<IMMDeviceCollection> endpoints;
    if (FAILED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &endpoints))) return;
    UINT count = 0;
    endpoints->GetCount(&count);
    for (UINT i = 0; i < count; ++i)
    {
        ComPtr<IMMDevice> endpoint;
        if (FAILED(endpoints->Item(i, &endpoint))) continue;
        LPWSTR id = nullptr;
        if (FAILED(endpoint->GetId(&id))) continue;
        juce::String name(id);
        CoTaskMemFree(id);
        ComPtr<IPropertyStore> properties;
        if (SUCCEEDED(endpoint->OpenPropertyStore(STGM_READ, &properties)))
        {
            PROPVARIANT value;
            PropVariantInit(&value);
            if (SUCCEEDED(properties->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR)
                name += " [" + juce::String(value.pwszVal) + "]";
            PropVariantClear(&value);
        }
        ComPtr<IAudioSessionManager2> manager;
        if (FAILED(endpoint->Activate(__uuidof(IAudioSessionManager2), CLSCTX_ALL, nullptr, &manager))) continue;
        ComPtr<IAudioSessionEnumerator> sessions;
        if (FAILED(manager->GetSessionEnumerator(&sessions))) continue;
        int sessionCount = 0;
        sessions->GetCount(&sessionCount);
        juce::Logger::writeToLog("Endpoint: " + name + " sessions=" + juce::String(sessionCount));
        for (int j = 0; j < sessionCount; ++j)
        {
            ComPtr<IAudioSessionControl> session;
            ComPtr<IAudioSessionControl2> details;
            if (FAILED(sessions->GetSession(j, &session)) || FAILED(session.As(&details))) continue;
            DWORD pid = 0;
            AudioSessionState state = AudioSessionStateInactive;
            details->GetProcessId(&pid);
            session->GetState(&state);
            juce::String processName;
            if (HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid))
            {
                wchar_t path[32768]; DWORD size = 32768;
                if (QueryFullProcessImageNameW(process, 0, path, &size))
                    processName = juce::File(juce::String(path)).getFileName();
                CloseHandle(process);
            }
            juce::Logger::writeToLog("  Session pid=" + juce::String(pid)
                + " process=" + processName + " state=" + juce::String(static_cast<int>(state)));
        }
    }
}
