#pragma once
#include "AsioEndpointSelection.h"
#include "RealtekOutputPolicy.h"

namespace fengyin::audioengine
{
struct RealtekConfigurationResult { bool configured = false, rollbackOK = true; };

// ASIO4ALL Private API 2.0. Do not use Windows endpoint topology or jack names.
inline RealtekConfigurationResult configureRealtekOutputs(void* driver)
{
    if (!driver) return {};
    const GUID iid {0xa26078c5,0x2840,0x4726,{0xb4,0x27,0xe6,0x0f,0xc8,0xfe,0xe4,0x03}};
    Microsoft::WRL::ComPtr<A4Private> api;
    const auto hr = static_cast<IUnknown*>(driver)->QueryInterface(iid,
        reinterpret_cast<void**>(api.GetAddressOf()));
    if (FAILED(hr))
    {
        juce::Logger::writeToLog("Realtek configuration: private API unavailable HRESULT="
            + juce::String::toHexString(static_cast<int>(hr)));
        return {};
    }
    struct Context
    {
        A4Private* api;
        bool attempted = false;
        bool verificationAttempted = false;
        RealtekConfigurationResult result;
        bool property(DWORD status)
        {
            if (status == 0) return true;
            juce::Logger::writeToLog("Realtek configuration: property error=0x"
                + juce::String::toHexString(static_cast<int>(status)));
            return false;
        }
        bool text(long d, int prop, juce::String& value)
        {
            const char* raw = nullptr;
            if (!property(api->getDevice(prop,d,&raw,sizeof(raw)))) return false;
            if (raw != nullptr && raw[0] != 0)
                value = raw[1] == 0 ? juce::String(reinterpret_cast<const wchar_t*>(raw))
                                   : juce::String::fromUTF8(raw);
            return true;
        }
        static BOOL run(void* data)
        {
            auto& c = *static_cast<Context*>(data);
            const bool verifying = c.attempted;
            if (verifying && c.verificationAttempted) return FALSE;
            if (verifying)
            {
                c.verificationAttempted = true;
                // A refresh must not silently undo the configuration. Until
                // this complete read succeeds, do not report a verified route.
                c.result.configured = false;
                c.result.rollbackOK = false;
            }
            c.attempted = true;
            std::vector<AsioRouteNode> nodes;
            bool devicesEnded = false;
            for (long d=0; d<128; ++d)
            {
                DWORD flags=0;
                const auto status=c.api->getDevice(0,d,&flags,4);
                if (status==0xA4AE1001u) { devicesEnded=true; break; }
                if (!c.property(status)) return FALSE;
                juce::String name,id;
                if (!c.text(d,1,name) || !c.text(d,5,id)) return FALSE;
                // Prefer a hardware PnP vendor identity; device-level fallback
                // supports driver names without depending on output pin labels.
                const bool physical=id.startsWithIgnoreCase("HDAUDIO\\")
                    || id.startsWithIgnoreCase("INTELAUDIO\\");
                const bool realtek=physical && (id.containsIgnoreCase("VEN_10EC")
                    || name.containsIgnoreCase("Realtek"));
                juce::Logger::writeToLog("Realtek enumeration device="+juce::String(d)
                    +" name="+name+" id="+id+" selected="+juce::String(realtek?1:0));
                nodes.push_back({d,-1,-1,flags,realtek,false,false});
                bool interfacesEnded=false;
                for (long i=0; i<128; ++i)
                {
                    const auto statusI=c.api->getInterface(0,d,i,&flags,4);
                    if (statusI==0xA4AE1001u) { interfacesEnded=true; break; }
                    if (!c.property(statusI)) return FALSE;
                    nodes.push_back({d,i,-1,flags,realtek,false,false});
                    bool pinsEnded=false;
                    for (long p=0; p<128; ++p)
                    {
                        const auto statusP=c.api->getPin(0,d,i,p,&flags,4);
                        if (statusP==0xA4AE1001u) { pinsEnded=true; break; }
                        DWORD flow=0,channels=0;
                        if (!c.property(statusP)
                            || !c.property(c.api->getPin(1,d,i,p,&flow,4))
                            || !c.property(c.api->getPin(2,d,i,p,&channels,4))) return FALSE;
                        const bool output=flow==KSPIN_DATAFLOW_IN;
                        const bool available=(flags&0x40000000u)!=0 && (flags&0x20000000u)==0 && channels>0;
                        nodes.push_back({d,i,p,flags,realtek,output,available});
                        juce::Logger::writeToLog("Realtek enumeration pin="+juce::String(d)+":"
                            +juce::String(i)+":"+juce::String(p)+" output="+juce::String(output?1:0)
                            +" channels="+juce::String(static_cast<int>(channels))+" flags="
                            +juce::String::toHexString(static_cast<int>(flags)));
                    }
                    if (!pinsEnded) return FALSE;
                }
                if (!interfacesEnded) return FALSE;
            }
            std::vector<AsioRouteChange> plan;
            if (!devicesEnded || !planRealtekOutputs(nodes,0x80000000u,plan))
            {
                juce::Logger::writeToLog("Realtek configuration: no complete usable route; unchanged");
                return FALSE;
            }
            if (verifying)
            {
                c.result.configured = plan.empty();
                c.result.rollbackOK = plan.empty();
                juce::Logger::writeToLog(plan.empty()
                    ? "Realtek configuration: verified after driver refresh"
                    : "Realtek configuration: driver refresh changed enabled state; stopping");
                return FALSE;
            }
            auto write=[&](const AsioRouteNode& n,std::uint32_t value) {
                DWORD v=value;
                return c.property(n.pin>=0 ? c.api->setPin(0,n.device,n.interfaceIndex,n.pin,&v,4)
                    : n.interfaceIndex>=0 ? c.api->setInterface(0,n.device,n.interfaceIndex,&v,4)
                    : c.api->setDevice(0,n.device,&v,4));
            };
            auto read=[&](const AsioRouteNode& n,std::uint32_t& value) {
                DWORD v=0;
                const auto status=n.pin>=0 ? c.api->getPin(0,n.device,n.interfaceIndex,n.pin,&v,4)
                    : n.interfaceIndex>=0 ? c.api->getInterface(0,n.device,n.interfaceIndex,&v,4)
                    : c.api->getDevice(0,n.device,&v,4);
                value=v;return c.property(status);
            };
            c.result.configured=applyRealtekPlan(plan,write,read,c.result.rollbackOK);
            juce::Logger::writeToLog("Realtek configuration: changes="+juce::String(static_cast<int>(plan.size()))
                +" verified="+juce::String(c.result.configured?1:0)
                +" rollbackOK="+juce::String(c.result.rollbackOK?1:0));
            return c.result.configured && !plan.empty();
        }
    } context {api.Get()};
    api->callback(&Context::run,&context);
    api->enumerate();
    api->callback(nullptr,nullptr);
    if (!context.attempted) juce::Logger::writeToLog("Realtek configuration: no enumeration callback");
    return context.result;
}
}
