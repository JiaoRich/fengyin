#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <devicetopology.h>
#include <ks.h>
#include <wrl/client.h>
#include <juce_core/juce_core.h>
#include <vector>

namespace fengyin::audioengine
{
// ABI defined by ASIO4ALL Private API v2.0, queried on the SAME IASIO
// instance. All property access is confined to its idle enumeration callback.
struct A4Private : IUnknown
{
    virtual DWORD version() = 0;
    virtual void enumerate() = 0;
    virtual void callback(BOOL (*)(void*), void*) = 0;
    virtual DWORD getDevice(int, long, void*, long) = 0;
    virtual DWORD setDevice(int, long, void*, long) = 0;
    virtual DWORD getInterface(int, long, long, void*, long) = 0;
    virtual DWORD setInterface(int, long, long, void*, long) = 0;
    virtual DWORD getPin(int, long, long, long, void*, long) = 0;
    virtual DWORD setPin(int, long, long, long, void*, long) = 0;
};

inline bool selectAsioEndpoint(void* driver, const std::wstring& endpoint)
{
    if (!driver || endpoint.empty()) return false;
    using Microsoft::WRL::ComPtr;
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;
    ComPtr<IDeviceTopology> topology;
    ComPtr<IConnector> connector, connected;
    ComPtr<IPart> part;
    if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            IID_PPV_ARGS(&enumerator)))
        || FAILED(enumerator->GetDevice(endpoint.c_str(), &device))
        || FAILED(device->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, nullptr,
            reinterpret_cast<void**>(topology.GetAddressOf())))
        || FAILED(topology->GetConnector(0, &connector))
        || FAILED(connector->GetConnectedTo(&connected))
        || FAILED(connected.As(&part))) return false;
    LPWSTR rawPath = nullptr;
    UINT localId = 0;
    if (FAILED(connector->GetDeviceIdConnectedTo(&rawPath))) return false;
    const juce::String path(rawPath);
    CoTaskMemFree(rawPath);
    if (FAILED(part->GetLocalId(&localId))) return false;
    const GUID iid {0xa26078c5,0x2840,0x4726,{0xb4,0x27,0xe6,0x0f,0xc8,0xfe,0xe4,0x03}};
    ComPtr<A4Private> api;
    if (FAILED(static_cast<IUnknown*>(driver)->QueryInterface(iid,
            reinterpret_cast<void**>(api.GetAddressOf())))) return false;
    struct Context
    {
        A4Private* api;
        juce::String path;
        long pin;
        bool attempted = false, matched = false;
        struct Flags { long d, i, p; DWORD oldValue; };
        std::vector<Flags> changed;
        static BOOL run(void* raw)
        {
            auto& c = *static_cast<Context*>(raw);
            if (c.attempted) return FALSE;
            c.attempted = true;
            long chosenD = -1, chosenI = -1;
            for (long d = 0; d < 128; ++d)
            {
                DWORD flags = 0;
                if (c.api->getDevice(0, d, &flags, 4) != 0) break;
                for (long i = 0; i < 128; ++i)
                {
                    const void* detail = nullptr;
                    if (c.api->getInterface(2, d, i, &detail, sizeof(detail)) != 0) break;
                    if (!detail) continue;
                    const auto* bytes = static_cast<const char*>(detail) + sizeof(DWORD);
                    const auto candidate = bytes[1] == 0
                        ? juce::String(reinterpret_cast<const wchar_t*>(bytes))
                        : juce::String::fromUTF8(bytes);
                    if (candidate.equalsIgnoreCase(c.path)) { chosenD = d; chosenI = i; }
                }
            }
            DWORD flow = 0, channels = 0, flags = 0;
            if (chosenD < 0
                || c.api->getPin(1, chosenD, chosenI, c.pin, &flow, 4) != 0 || flow != KSPIN_DATAFLOW_IN
                || c.api->getPin(2, chosenD, chosenI, c.pin, &channels, 4) != 0 || channels < 2
                || c.api->getPin(0, chosenD, chosenI, c.pin, &flags, 4) != 0
                || (flags & 0x20000000u) != 0) return FALSE;
            // Match the KS interface AND pin before changing anything.
            auto change = [&](long d, long i, long p, bool enabled)
            {
                DWORD old = 0;
                const auto read = p >= 0 ? c.api->getPin(0,d,i,p,&old,4)
                    : i >= 0 ? c.api->getInterface(0,d,i,&old,4) : c.api->getDevice(0,d,&old,4);
                if (read != 0) return false;
                DWORD value = enabled ? old | 0x80000000u : old & ~0x80000000u;
                if (old == value) return true;
                c.changed.push_back({d,i,p,old});
                return (p >= 0 ? c.api->setPin(0,d,i,p,&value,4)
                    : i >= 0 ? c.api->setInterface(0,d,i,&value,4) : c.api->setDevice(0,d,&value,4)) == 0;
            };
            bool ok = change(chosenD,-1,-1,true) && change(chosenD,chosenI,-1,true);
            for (long d=0; ok && d<128; ++d)
            {
                DWORD value=0;
                if (c.api->getDevice(0,d,&value,4)!=0) break;
                for (long i=0; ok && i<128; ++i)
                {
                    if (c.api->getInterface(0,d,i,&value,4)!=0) break;
                    for (long p=0; ok && p<128; ++p)
                    {
                        if (c.api->getPin(1,d,i,p,&value,4)!=0) break;
                        if (value==KSPIN_DATAFLOW_IN) ok = change(d,i,p,d==chosenD && i==chosenI && p==c.pin);
                    }
                }
            }
            if (!ok)
                for (auto it=c.changed.rbegin(); it!=c.changed.rend(); ++it)
                    if (it->p>=0) c.api->setPin(0,it->d,it->i,it->p,&it->oldValue,4);
                    else if (it->i>=0) c.api->setInterface(0,it->d,it->i,&it->oldValue,4);
                    else c.api->setDevice(0,it->d,&it->oldValue,4);
            c.matched=ok;
            return !c.changed.empty(); // One bounded refresh, never recursive.
        }
    } context {api.Get(),path,static_cast<long>(localId & 0xffff)};
    api->callback(&Context::run,&context);
    api->enumerate();
    api->callback(nullptr,nullptr);
    return context.matched;
}
}
