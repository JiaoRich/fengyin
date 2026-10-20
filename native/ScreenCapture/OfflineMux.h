#pragma once
#include "GpuVideo.h"
#include <filesystem>
#include <cstring>

namespace fengyin::screen {
// Called only after capture ends. H264 is copied as compressed samples; only
// PCM -> AAC is encoded here. Never remove source files on success or failure.
inline void mux(const std::filesystem::path& video,const std::filesystem::path& wave,
                const std::filesystem::path& destination,std::int64_t audioTrim,std::int64_t duration){
    if(audioTrim<0||duration<=0)throw std::runtime_error("Invalid audio/video alignment");
    if(std::filesystem::exists(destination))throw std::runtime_error("Refusing to overwrite recording");
    ComPtr<IMFSourceReader> picture,sound;
    checked(MFCreateSourceReaderFromURL(video.c_str(),nullptr,&picture));
    checked(MFCreateSourceReaderFromURL(wave.c_str(),nullptr,&sound));
    checked(picture->SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS,FALSE));
    checked(picture->SetStreamSelection(MF_SOURCE_READER_FIRST_VIDEO_STREAM,TRUE));
    ComPtr<IMFMediaType> h264,pcm,aac;
    checked(picture->GetCurrentMediaType(MF_SOURCE_READER_FIRST_VIDEO_STREAM,&h264));
    GUID codec{};checked(h264->GetGUID(MF_MT_SUBTYPE,&codec));
    if(codec!=MFVideoFormat_H264)throw std::runtime_error("Mux input is not compressed H264");
    checked(sound->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM,&pcm));
    checked(pcm->GetGUID(MF_MT_SUBTYPE,&codec));
    UINT32 rate=0,channels=0,bits=0;
    checked(pcm->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,&rate));
    checked(pcm->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS,&channels));checked(pcm->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,&bits));
    if(codec!=MFAudioFormat_PCM||rate!=48000||channels!=2||bits!=16)throw std::runtime_error("Unexpected PCM format; no hidden conversion");
    ComPtr<IMFSinkWriter> out;checked(MFCreateSinkWriterFromURL(destination.c_str(),nullptr,nullptr,&out));
    DWORD v=0,a=0;checked(out->AddStream(h264.Get(),&v));checked(out->SetInputMediaType(v,h264.Get(),nullptr));
    checked(MFCreateMediaType(&aac));checked(aac->SetGUID(MF_MT_MAJOR_TYPE,MFMediaType_Audio));
    checked(aac->SetGUID(MF_MT_SUBTYPE,MFAudioFormat_AAC));checked(aac->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND,48000));
    checked(aac->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS,2));checked(aac->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE,16));
    checked(aac->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND,24000));checked(aac->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE,0));
    checked(out->AddStream(aac.Get(),&a));checked(out->SetInputMediaType(a,pcm.Get(),nullptr));checked(out->BeginWriting());
    // Interleave reads by media time. This prevents the mux from retaining an
    // entire video stream while it waits for the first audio sample.
    std::int64_t vt=0,at=0,writtenFrames=0,readFrames=0;bool ve=false,ae=false;
    const auto trimFrames=(audioTrim*48000+second/2)/second;
    const auto targetFrames=duration*48000/second;
    while(!ve||!ae){
        const bool takeVideo=!ve&&(ae||vt<=at);
        ComPtr<IMFSample> sample;DWORD flags=0;LONGLONG time=0;
        auto* reader=takeVideo?picture.Get():sound.Get();
        checked(reader->ReadSample(takeVideo?MF_SOURCE_READER_FIRST_VIDEO_STREAM:MF_SOURCE_READER_FIRST_AUDIO_STREAM,0,nullptr,&flags,&time,&sample));
        if(flags&MF_SOURCE_READERF_ERROR)throw std::runtime_error("Mux source read failed");
        if(flags&(MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED|MF_SOURCE_READERF_NATIVEMEDIATYPECHANGED))throw std::runtime_error("Mux input format changed");
        if(flags&MF_SOURCE_READERF_ENDOFSTREAM){if(takeVideo)ve=true;else ae=true;continue;}
        if(!sample)continue;
        if(takeVideo){
            checked(sample->GetSampleTime(&time));vt=time;
            if(time<duration)checked(out->WriteSample(v,sample.Get()));else ve=true;
        }else{
            ComPtr<IMFMediaBuffer> buffer;checked(sample->ConvertToContiguousBuffer(&buffer));
            BYTE* data=nullptr;DWORD bytes=0;checked(buffer->Lock(&data,nullptr,&bytes));
            const auto count=std::int64_t(bytes/4);
            const auto skip=std::clamp(trimFrames-readFrames,std::int64_t(0),count);
            const auto keep=std::max(std::int64_t(0),std::min(count-skip,targetFrames-writtenFrames));
            if(keep){
                ComPtr<IMFMediaBuffer> copy;auto hr=MFCreateMemoryBuffer(DWORD(keep*4),&copy);
                if(FAILED(hr)){buffer->Unlock();checked(hr);}
                BYTE* dest=nullptr;hr=copy->Lock(&dest,nullptr,nullptr);
                if(FAILED(hr)){buffer->Unlock();checked(hr);}
                std::memcpy(dest,data+skip*4,std::size_t(keep*4));copy->Unlock();buffer->Unlock();
                checked(copy->SetCurrentLength(DWORD(keep*4)));ComPtr<IMFSample> packet;checked(MFCreateSample(&packet));
                checked(packet->AddBuffer(copy.Get()));checked(packet->SetSampleTime(writtenFrames*second/48000));
                checked(packet->SetSampleDuration(keep*second/48000));checked(out->WriteSample(a,packet.Get()));writtenFrames+=keep;
            }else buffer->Unlock();
            readFrames+=count;at=writtenFrames*second/48000;
            if(writtenFrames>=targetFrames)ae=true;
        }
    }
    if(writtenFrames<targetFrames)throw std::runtime_error("PCM does not cover video duration; refusing silent padding");
    checked(out->Finalize());
}
}
