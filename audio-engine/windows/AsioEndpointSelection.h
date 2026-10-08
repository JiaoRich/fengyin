#pragma once
#include <windows.h>
#include <mmdeviceapi.h>
#include <devicetopology.h>
#include <ks.h>
#include <wrl/client.h>
#include <juce_core/juce_core.h>
#include <vector>
#include <set>
#include <functional>

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

// Re-enumeration is also needed when endpoint identity matching is unavailable.
// Call only after closing buffers and before opening the stream, never from an
// audio callback. This refreshes jack state without displaying a control panel.
inline bool refreshAsioEndpoints(void* driver)
{
    if (!driver) return false;
    const GUID iid {0xa26078c5,0x2840,0x4726,{0xb4,0x27,0xe6,0x0f,0xc8,0xfe,0xe4,0x03}};
    Microsoft::WRL::ComPtr<A4Private> api;
    if (FAILED(static_cast<IUnknown*>(driver)->QueryInterface(iid,
            reinterpret_cast<void**>(api.GetAddressOf())))) return false;
    api->enumerate();
    return true;
}

inline bool selectAsioEndpoint(void* driver, const std::wstring& endpoint)
{
    const auto checked = [](const char* stage, HRESULT result)
    {
        if (SUCCEEDED(result)) return true;
        juce::Logger::writeToLog(juce::String("ASIO endpoint failure: ") + stage
            + " HRESULT=0x" + juce::String::toHexString(static_cast<int>(result)));
        return false;
    };
    if (!driver || endpoint.empty())
    {
        juce::Logger::writeToLog(!driver ? "ASIO endpoint failure: null driver"
                                       : "ASIO endpoint failure: empty requested endpoint");
        return false;
    }
    juce::Logger::writeToLog("ASIO requested endpoint=" + juce::String(endpoint.c_str()));
    using Microsoft::WRL::ComPtr;
    ComPtr<IMMDeviceEnumerator> enumerator;
    ComPtr<IMMDevice> device;
    ComPtr<IDeviceTopology> topology;
    ComPtr<IConnector> connector, connected;
    ComPtr<IPart> part;
    if (!checked("CoCreateInstance(MMDeviceEnumerator)", CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
            IID_PPV_ARGS(&enumerator)))
        || !checked("GetDevice(requested endpoint)", enumerator->GetDevice(endpoint.c_str(), &device))
        || !checked("Activate(IDeviceTopology)", device->Activate(__uuidof(IDeviceTopology), CLSCTX_ALL, nullptr,
            reinterpret_cast<void**>(topology.GetAddressOf())))
        || !checked("GetConnector(0)", topology->GetConnector(0, &connector))
        || !checked("GetConnectedTo(adapter)", connector->GetConnectedTo(&connected))
        || !checked("QueryInterface(IPart)", connected.As(&part))) return false;
    // The endpoint's adjacent connector is usually a topology/jack pin, NOT
    // the wave filter's streaming pin. Follow the render path upstream to
    // Software_IO before comparing an interface/pin with ASIO4ALL.
    struct StreamPin { juce::String path; long pin; };
    std::vector<StreamPin> streams;
    std::set<std::wstring> visited;
    bool complete = true;
    std::function<void(IPart*)> walk = [&](IPart* current)
    {
        if (!complete) return;
        LPWSTR global = nullptr;
        if (FAILED(current->GetGlobalId(&global))) { complete = false; return; }
        const std::wstring key(global);
        CoTaskMemFree(global);
        if (!visited.insert(key).second) return;
        if (visited.size() > 256) { complete = false; return; }
        ComPtr<IConnector> edge;
        if (SUCCEEDED(current->QueryInterface(IID_PPV_ARGS(&edge))))
        {
            ConnectorType kind;
            DataFlow flow;
            if (FAILED(edge->GetType(&kind)) || FAILED(edge->GetDataFlow(&flow)))
                { complete = false; return; }
            if (kind == Software_IO)
            {
                if (flow != In) return; // Render streams enter the adapter.
                ComPtr<IDeviceTopology> owner;
                LPWSTR rawPath = nullptr;
                UINT localId = 0;
                if (FAILED(current->GetTopologyObject(&owner))
                    || FAILED(current->GetLocalId(&localId))
                    || FAILED(owner->GetDeviceId(&rawPath)))
                    { complete = false; return; }
                streams.push_back({juce::String(rawPath), static_cast<long>(localId & 0xffff)});
                CoTaskMemFree(rawPath);
                return;
            }
            if (kind == Software_Fixed && flow == In)
            {
                ComPtr<IConnector> upstream;
                ComPtr<IPart> upstreamPart;
                if (FAILED(edge->GetConnectedTo(&upstream)) || FAILED(upstream.As(&upstreamPart)))
                    { complete = false; return; }
                walk(upstreamPart.Get());
                return;
            }
        }
        ComPtr<IPartsList> incoming;
        UINT count = 0;
        const auto hr = current->EnumPartsIncoming(&incoming);
        if (hr == E_NOTFOUND) return;
        if (FAILED(hr) || FAILED(incoming->GetCount(&count))) { complete = false; return; }
        for (UINT i = 0; i < count; ++i)
        {
            ComPtr<IPart> next;
            if (FAILED(incoming->GetPart(i, &next))) { complete = false; return; }
            walk(next.Get());
        }
    };
    walk(part.Get());
    // An ambiguous/offload topology is not permission to guess another output.
    if (!complete || streams.size() != 1)
    {
        juce::Logger::writeToLog("ASIO endpoint resolution: incomplete or ambiguous render topology, streams="
            + juce::String(static_cast<int>(streams.size())));
        return false;
    }
    const auto& stream = streams.front();
    juce::Logger::writeToLog("ASIO resolved stream=" + stream.path + " pin=" + juce::String(stream.pin));
    const GUID iid {0xa26078c5,0x2840,0x4726,{0xb4,0x27,0xe6,0x0f,0xc8,0xfe,0xe4,0x03}};
    ComPtr<A4Private> api;
    if (!checked("QueryInterface(ASIO4ALL private API)", static_cast<IUnknown*>(driver)->QueryInterface(iid,
            reinterpret_cast<void**>(api.GetAddressOf())))) return false;
    struct Context
    {
        A4Private* api;
        juce::String path;
        long pin;
        bool attempted = false, matched = false;
        struct Flags { long d, i, p; DWORD oldValue; };
        std::vector<Flags> changed;
        static bool propertyOK(const char* stage, DWORD result, bool endAllowed = false)
        {
            if (result == 0) return true;
            if (!(endAllowed && result == 0xA4AE1001u))
                juce::Logger::writeToLog(juce::String("ASIO private failure: ") + stage
                    + " code=0x" + juce::String::toHexString(static_cast<int>(result)));
            return false;
        }
        static BOOL run(void* raw)
        {
            auto& c = *static_cast<Context*>(raw);
            if (c.attempted) return FALSE;
            c.attempted = true;
            long chosenD = -1, chosenI = -1;
            for (long d = 0; d < 128; ++d)
            {
                DWORD flags = 0;
                if (!propertyOK("getDevice(flags)", c.api->getDevice(0, d, &flags, 4), true)) break;
                for (long i = 0; i < 128; ++i)
                {
                    const void* detail = nullptr;
                    if (!propertyOK("getInterface(path)", c.api->getInterface(2, d, i, &detail, sizeof(detail)), true)) break;
                    if (!detail) continue;
                    const auto* bytes = static_cast<const char*>(detail) + sizeof(DWORD);
                    const auto candidate = bytes[1] == 0
                        ? juce::String(reinterpret_cast<const wchar_t*>(bytes))
                        : juce::String::fromUTF8(bytes);
                    juce::Logger::writeToLog("ASIO candidate d=" + juce::String(d)
                        + " i=" + juce::String(i) + " path=" + candidate);
                    if (candidate.equalsIgnoreCase(c.path)) { chosenD = d; chosenI = i; }
                }
            }
            DWORD flow = 0, channels = 0, flags = 0;
            if (chosenD < 0)
            {
                juce::Logger::writeToLog("ASIO endpoint failure: no interface path match");
                return FALSE;
            }
            if (!propertyOK("getPin(flow)", c.api->getPin(1, chosenD, chosenI, c.pin, &flow, 4))
                || !propertyOK("getPin(channels)", c.api->getPin(2, chosenD, chosenI, c.pin, &channels, 4))
                || !propertyOK("getPin(flags)", c.api->getPin(0, chosenD, chosenI, c.pin, &flags, 4))) return FALSE;
            juce::Logger::writeToLog("ASIO matched pin flow=" + juce::String(static_cast<int>(flow))
                + " channels=" + juce::String(static_cast<int>(channels))
                + " flags=0x" + juce::String::toHexString(static_cast<int>(flags)));
            if (flow != KSPIN_DATAFLOW_IN || channels < 2 || (flags & 0x20000000u) != 0)
            {
                juce::Logger::writeToLog("ASIO endpoint failure: matched pin is not usable stereo render");
                return FALSE;
            }
            // Match the KS interface AND pin before changing anything.
            auto change = [&](long d, long i, long p, bool enabled)
            {
                DWORD old = 0;
                const auto read = p >= 0 ? c.api->getPin(0,d,i,p,&old,4)
                    : i >= 0 ? c.api->getInterface(0,d,i,&old,4) : c.api->getDevice(0,d,&old,4);
                if (!propertyOK("read flags before change", read)) return false;
                DWORD value = enabled ? old | 0x80000000u : old & ~0x80000000u;
                if (old == value) return true;
                c.changed.push_back({d,i,p,old});
                return propertyOK("write enabled flags", p >= 0 ? c.api->setPin(0,d,i,p,&value,4)
                    : i >= 0 ? c.api->setInterface(0,d,i,&value,4) : c.api->setDevice(0,d,&value,4));
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
    } context {api.Get(),stream.path,stream.pin};
    api->callback(&Context::run,&context);
    api->enumerate();
    api->callback(nullptr,nullptr);
    if (!context.attempted)
        juce::Logger::writeToLog("ASIO endpoint failure: enumeration callback was not invoked");
    return context.matched;
}
}
