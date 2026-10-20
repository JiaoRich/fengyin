#pragma once
#include <audioclient.h>
#include <mmdeviceapi.h>
#include <avrt.h>
#include <array>
#include <atomic>
#include <cstring>
#include <filesystem>
#include <future>
#include <mutex>
#include <thread>
#include <vector>
#include "GpuVideo.h"
#include "../ScreenRecorder/PcmWaveWriter.h"

namespace fengyin::screen {
// Experimental helper only: not a replacement for the release WAV recorder.
// Endpoint QPC comes from GetBuffer, not callback-arrival time. The same 48k,
// stereo, s16 format as the current verified release recorder is retained.
class TimedAudio {
    static constexpr std::size_t capacity=64,packetFrames=2048;
    struct Packet {std::array<std::int16_t,packetFrames*2> pcm;UINT32 frames=0;};
    std::array<Packet,capacity> queue{};
    std::atomic<std::uint64_t> produced{0},consumed{0};
    std::atomic<bool> stopping{false},producerDone{false},failed{false};
    std::thread producer,consumer;
    std::mutex errorMutex;std::string failure;
    std::atomic<std::int64_t> first{0},last{0};
    std::atomic<std::uint64_t> total{0};
    void fail(std::string message){std::lock_guard lock(errorMutex);if(failure.empty())failure=std::move(message);failed.store(true);stopping.store(true);}
public:
    std::int64_t firstQpc()const{return first.load();}
    std::int64_t endQpc()const{return last.load();}
    std::uint64_t frames()const{return total.load();}
    bool hasFailed()const{return failed.load();}
    void checkError(){if(failed.load()){std::lock_guard lock(errorMutex);throw std::runtime_error(failure);}}
    void start(const std::filesystem::path& path){
        std::promise<void> ready;auto future=ready.get_future();
        consumer=std::thread([this,path]{
            try{
                PcmWaveWriter writer(path);std::vector<std::int16_t> pcm(packetFrames*2);
                for(;;){
                    const auto index=consumed.load(std::memory_order_relaxed);
                    if(index==produced.load(std::memory_order_acquire)){
                        if(producerDone.load()&&index==produced.load(std::memory_order_acquire))break;
                        Sleep(3);continue;
                    }
                    const auto& packet=queue[index%capacity];pcm.resize(packet.frames*2);
                    std::memcpy(pcm.data(),packet.pcm.data(),pcm.size()*2);writer.append(pcm);
                    consumed.store(index+1,std::memory_order_release);
                }
                writer.close();
            }catch(const std::exception& e){fail(e.what());}
        });
        producer=std::thread([this,ready=std::move(ready)]()mutable{
            bool announced=false;HANDLE task=nullptr;
            try{
                winrt::init_apartment(winrt::apartment_type::multi_threaded);
                DWORD taskId=0;task=AvSetMmThreadCharacteristicsW(L"Audio",&taskId);
                ComPtr<IMMDeviceEnumerator> enumerator;ComPtr<IMMDevice> device;
                checked(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&enumerator)));
                checked(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&device));
                const auto getId=[](IMMDevice* d){LPWSTR p=nullptr;checked(d->GetId(&p));std::wstring id=p;CoTaskMemFree(p);return id;};
                const auto endpoint=getId(device.Get());
                ComPtr<IAudioClient> capture,clock;
                checked(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(capture.GetAddressOf())));
                checked(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(clock.GetAddressOf())));
                WAVEFORMATEX format{WAVE_FORMAT_PCM,2,48000,192000,4,16,0};
                constexpr DWORD convert=AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
                checked(capture->Initialize(AUDCLNT_SHAREMODE_SHARED,AUDCLNT_STREAMFLAGS_LOOPBACK|convert,1000000,0,&format,nullptr));
                checked(clock->Initialize(AUDCLNT_SHAREMODE_SHARED,convert,1000000,0,&format,nullptr));
                ComPtr<IAudioCaptureClient> input;ComPtr<IAudioRenderClient> silent;
                checked(capture->GetService(IID_PPV_ARGS(&input)));checked(clock->GetService(IID_PPV_ARGS(&silent)));
                UINT32 bufferFrames=0;checked(clock->GetBufferSize(&bufferFrames));
                const auto feedSilence=[&]{UINT32 used=0;checked(clock->GetCurrentPadding(&used));if(used<bufferFrames){
                    BYTE* ignored=nullptr;checked(silent->GetBuffer(bufferFrames-used,&ignored));
                    checked(silent->ReleaseBuffer(bufferFrames-used,AUDCLNT_BUFFERFLAGS_SILENT));}};
                feedSilence();checked(clock->Start());checked(capture->Start());
                struct StopClients{IAudioClient* a;IAudioClient* b;~StopClients(){a->Stop();b->Stop();}} stop{capture.Get(),clock.Get()};
                ready.set_value();announced=true;std::uint64_t nextPosition=0;bool seen=false;
                auto nextCheck=GetTickCount64()+1000;
                while(!stopping.load()){
                    feedSilence();UINT32 available=0;checked(input->GetNextPacketSize(&available));
                    for(int packets=0;available&&packets<32;++packets){
                        BYTE* data=nullptr;UINT32 count=0;DWORD flags=0;UINT64 position=0,stamp=0;
                        checked(input->GetBuffer(&data,&count,&flags,&position,&stamp));
                        struct Release{IAudioCaptureClient* p;UINT32 n;bool active=true;
                            ~Release(){if(active)p->ReleaseBuffer(n);}
                            void now(){checked(p->ReleaseBuffer(n));active=false;}} release{input.Get(),count};
                        if((flags&AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR)||stamp==0)throw std::runtime_error("Audio timestamp unavailable; no guessed sync");
                        if(seen&&((flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY)||position!=nextPosition))throw std::runtime_error("Audio discontinuity detected; partial recording retained");
                        if(count>packetFrames)throw std::runtime_error("Audio packet exceeds bounded queue slot");
                        const auto index=produced.load(std::memory_order_relaxed);
                        if(index-consumed.load(std::memory_order_acquire)>=capacity)throw std::runtime_error("Audio disk queue overflow; partial recording retained");
                        auto& packet=queue[index%capacity];packet.frames=count;
                        if(flags&AUDCLNT_BUFFERFLAGS_SILENT)std::memset(packet.pcm.data(),0,count*4);
                        else{if(!data)throw std::runtime_error("Missing audio packet");std::memcpy(packet.pcm.data(),data,count*4);}
                        if(!seen)first.store(stamp);
                        nextPosition=position+count;seen=true;
                        last.store(stamp+std::int64_t(count)*second/audioRate);total.fetch_add(count);
                        produced.store(index+1,std::memory_order_release);
                        // Release before asking for the next packet. Never wait
                        // for disk or video while an endpoint buffer is held.
                        release.now();checked(input->GetNextPacketSize(&available));
                    }
                    if(GetTickCount64()>=nextCheck){ComPtr<IMMDevice> current;
                        checked(enumerator->GetDefaultAudioEndpoint(eRender,eMultimedia,&current));
                        if(getId(current.Get())!=endpoint)throw std::runtime_error("Default output changed; recording stopped");
                        nextCheck=GetTickCount64()+1000;
                    }
                    Sleep(2);
                }
            }catch(const winrt::hresult_error& e){fail(winrt::to_string(e.message()));if(!announced)ready.set_exception(std::current_exception());}
             catch(const std::exception& e){fail(e.what());if(!announced)ready.set_exception(std::current_exception());}
            if(task)AvRevertMmThreadCharacteristics(task);
            winrt::uninit_apartment();producerDone.store(true);
        });
        future.get();
    }
    void stop(){stopping.store(true);if(producer.joinable())producer.join();if(consumer.joinable())consumer.join();}
    ~TimedAudio(){stop();}
};
}
