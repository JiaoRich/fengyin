#define MA_ENABLE_ONLY_SPECIFIC_BACKENDS
#define MA_ENABLE_WASAPI
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH
#define MA_NO_ENGINE
// In 0.11.23 MA_HAS_WAV is defined with the decoder registration; retain WAV
// support for encoding and the final header/length check. No decoder runs while
// recording, and unnecessary FLAC/MP3 code is omitted.
#define MA_NO_FLAC
#define MA_NO_MP3
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>
#include "MiniAudioRecorder.h"
#include <atomic>
#include <algorithm>
#include <iterator>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace {
void checked(ma_result result,const char* operation) {
    if(result!=MA_SUCCESS)throw std::runtime_error(std::string(operation)+": "+ma_result_description(result));
}
}
struct MiniAudioRecorder::Impl {
    ma_context context{};
    ma_device capture{},silence{};
    ma_encoder encoder{};
    ma_pcm_rb queue{};
    bool contextReady=false,captureReady=false,silenceReady=false,encoderReady=false,queueReady=false;
    std::atomic<bool> stopping{false},failed{false};
    std::atomic<bool> recordingEnabled{true};
    uint64_t written=0;
    std::filesystem::path outputPath;
    void initWriter(const std::filesystem::path& path) {
        outputPath=path;
        checked(ma_pcm_rb_init(ma_format_s16,2,48000*10,nullptr,nullptr,&queue),"Recording queue");queueReady=true;
        auto format=ma_encoder_config_init(ma_encoding_format_wav,ma_format_s16,2,48000);
       #if defined(_WIN32)
        checked(ma_encoder_init_file_w(path.c_str(),&format,&encoder),"WAV open");
       #else
        checked(ma_encoder_init_file(path.c_str(),&format,&encoder),"WAV open");
       #endif
        encoderReady=true;
    }
    // Single producer, no locks/allocation/disk IO on miniaudio's capture callback.
    void push(const void* input,ma_uint32 count) noexcept {
        if(!recordingEnabled.load(std::memory_order_acquire))return;
        const auto* pcm=static_cast<const int16_t*>(input);
        while(count) {
            auto frames=count;void* target=nullptr;
            if(ma_pcm_rb_acquire_write(&queue,&frames,&target)!=MA_SUCCESS || frames==0){failed.store(true);return;}
            if(pcm){std::memcpy(target,pcm,frames*4);pcm+=frames*2;}
            else std::memset(target,0,frames*4);
            ma_pcm_rb_commit_write(&queue,frames);count-=frames;
        }
    }
    void drain() {
        while(ma_pcm_rb_available_read(&queue)) {
            ma_uint32 frames=8192;void* source=nullptr;
            checked(ma_pcm_rb_acquire_read(&queue,&frames,&source),"Recording queue read");
            if(!frames)break;
            if((written+frames)*4>UINT32_MAX-128ULL)throw std::runtime_error("WAV size limit reached");
            ma_uint64 actual=0;
            checked(ma_encoder_write_pcm_frames(&encoder,source,frames,&actual),"WAV write");
            if(actual!=frames)throw std::runtime_error("Incomplete WAV write");
            written+=actual;ma_pcm_rb_commit_read(&queue,frames);
        }
        if(failed.load())throw std::runtime_error("Audio device stopped or recording queue overflowed; partial file retained");
    }
    void stopDevices() {
        stopping.store(true);
        if(captureReady){ma_device_uninit(&capture);captureReady=false;}
        if(silenceReady){ma_device_uninit(&silence);silenceReady=false;}
    }
    ~Impl(){stopDevices();if(encoderReady)ma_encoder_uninit(&encoder);
        if(queueReady)ma_pcm_rb_uninit(&queue);if(contextReady)ma_context_uninit(&context);}
};
MiniAudioRecorder::MiniAudioRecorder():impl(std::make_unique<Impl>()){}
MiniAudioRecorder::~MiniAudioRecorder()=default;
void MiniAudioRecorder::start(const std::filesystem::path& output,const std::wstring& endpoint) {
    auto& p=*impl;p.initWriter(output);
   #if defined(_WIN32)
    const ma_backend backend=ma_backend_wasapi;
    checked(ma_context_init(&backend,1,nullptr,&p.context),"WASAPI context");p.contextReady=true;
    ma_device_id id{};
    if(endpoint.size()>=std::size(id.wasapi))throw std::runtime_error("Audio endpoint ID too long");
    std::copy(endpoint.begin(),endpoint.end(),id.wasapi);
    auto config=ma_device_config_init(ma_device_type_loopback);
    config.capture.pDeviceID=&id;config.capture.format=ma_format_s16;config.capture.channels=2;
    config.capture.shareMode=ma_share_mode_shared;config.sampleRate=48000;
    config.periodSizeInMilliseconds=20;config.wasapi.noAutoStreamRouting=MA_TRUE;
    config.wasapi.usage=ma_wasapi_usage_pro_audio;config.pUserData=&p;
    config.dataCallback=[](ma_device* d,void*,const void* in,ma_uint32 count){static_cast<Impl*>(d->pUserData)->push(in,count);};
    config.notificationCallback=[](const ma_device_notification* event){
        auto& data=*static_cast<Impl*>(event->pDevice->pUserData);
        if(!data.stopping.load() && (event->type==ma_device_notification_type_stopped
            || event->type==ma_device_notification_type_rerouted
            || event->type==ma_device_notification_type_interruption_began))data.failed.store(true);
    };
    checked(ma_device_init(&p.context,&config,&p.capture),"System output capture");p.captureReady=true;
    // WASAPI loopback produces no packets when *all* render streams stop.
    // A shared all-zero stream keeps sample time continuous during silence;
    // no audible content, microphone, device change or instrument-buffer change.
    auto keepalive=ma_device_config_init(ma_device_type_playback);
    keepalive.playback.pDeviceID=&id;keepalive.playback.format=ma_format_s16;keepalive.playback.channels=2;
    keepalive.playback.shareMode=ma_share_mode_shared;keepalive.sampleRate=48000;
    keepalive.periodSizeInMilliseconds=20;keepalive.wasapi.noAutoStreamRouting=MA_TRUE;
    keepalive.dataCallback=[](ma_device*,void* out,const void*,ma_uint32 count){std::memset(out,0,count*4);};
    checked(ma_device_init(&p.context,&keepalive,&p.silence),"Recording silent clock");p.silenceReady=true;
    checked(ma_device_start(&p.silence),"Recording clock start");
    checked(ma_device_start(&p.capture),"Recording capture start");
   #else
    (void)endpoint;throw std::runtime_error("System recording is Windows-only");
   #endif
}
void MiniAudioRecorder::drain(){impl->drain();}
void MiniAudioRecorder::setRecordingEnabled(bool enabled){impl->recordingEnabled.store(enabled,std::memory_order_release);}
void MiniAudioRecorder::finish(){
    auto& p=*impl;p.stopDevices();p.drain();
    ma_encoder_uninit(&p.encoder);p.encoderReady=false;
    // Upstream uninit returns void. Read the finished header back so failed
    // final length/header writes cannot be reported as a successful recording.
    ma_decoder decoder{};
   #if defined(_WIN32)
    checked(ma_decoder_init_file_w(p.outputPath.c_str(),nullptr,&decoder),"WAV header verification");
   #else
    checked(ma_decoder_init_file(p.outputPath.c_str(),nullptr,&decoder),"WAV header verification");
   #endif
    ma_uint64 frames=0;const auto result=ma_decoder_get_length_in_pcm_frames(&decoder,&frames);
    const bool valid=decoder.outputChannels==2 && decoder.outputSampleRate==48000;
    ma_decoder_uninit(&decoder);checked(result,"WAV length verification");
    if(!valid || frames!=p.written || frames==0)throw std::runtime_error("Incomplete WAV header or length");
}
void MiniAudioRecorder::selfTest(const std::filesystem::path& output){
    MiniAudioRecorder recorder;auto& p=*recorder.impl;p.initWriter(output);
    std::vector<int16_t> pcm(2000*2);
    // Preparation audio (including announcements/beep) must not enter the WAV.
    recorder.setRecordingEnabled(false);p.push(pcm.data(),2000);
    if(ma_pcm_rb_available_read(&p.queue)!=0)throw std::runtime_error("Preparation audio entered recording queue");
    recorder.setRecordingEnabled(true);
    for(int block=0;block<48;++block){
        for(int i=0;i<2000;++i){const auto v=static_cast<int16_t>(std::sin((block*2000+i)*6.283185307179586*440/48000)*3000);
            pcm[i*2]=pcm[i*2+1]=v;}
        p.push(pcm.data(),2000);
        if(block%3==2)p.drain(); // callback chunks != disk chunks; exact sample order.
    }
    recorder.finish();
    if(p.written!=96000)throw std::runtime_error("miniaudio selftest lost samples");
}
