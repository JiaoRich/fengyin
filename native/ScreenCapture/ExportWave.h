#pragma once
#include "../ScreenRecorder/PcmWaveWriter.h"
#include <array>
#include <algorithm>
#include <cstring>
namespace fengyin::screen {
// Our capture WAV is canonical 44-byte 48k stereo s16 PCM. Copy exactly the
// same sample range used by the MP4 mux; no resampling, gain or added silence.
inline void exportWave(const std::filesystem::path& source,const std::filesystem::path& target,
                       std::int64_t trim,std::int64_t duration){
    if(trim<0||trim>864000000000LL||duration<=0||duration>216000000000LL||std::filesystem::exists(target))throw std::runtime_error("Invalid WAV export or destination exists");
    std::ifstream in(source,std::ios::binary);std::array<unsigned char,44> h{};
    in.read(reinterpret_cast<char*>(h.data()),44);
    const auto u32=[&](int p){return std::uint32_t(h[p])|(std::uint32_t(h[p+1])<<8)|(std::uint32_t(h[p+2])<<16)|(std::uint32_t(h[p+3])<<24);};
    if(!in||std::memcmp(h.data(),"RIFF",4)||std::memcmp(h.data()+8,"WAVEfmt ",8)
       ||u32(16)!=16||h[20]!=1||h[21]!=0||h[22]!=2||h[23]!=0||u32(24)!=48000
       ||h[32]!=4||h[33]!=0||h[34]!=16||h[35]!=0||std::memcmp(h.data()+36,"data",4))
        throw std::runtime_error("Unexpected source WAV layout");
    const auto first=(trim*48000+5000000)/10000000,count=duration*48000/10000000;
    if(count<=0||first+count>u32(40)/4||std::filesystem::file_size(source)<44ULL+u32(40))
        throw std::runtime_error("WAV does not cover video duration");
    in.seekg(44+first*4);PcmWaveWriter out(target);std::vector<std::int16_t> pcm(4800*2);
    for(auto remaining=count;remaining>0;){
        const auto n=std::min<std::int64_t>(remaining,4800);pcm.resize(std::size_t(n)*2);
        in.read(reinterpret_cast<char*>(pcm.data()),n*4);if(!in)throw std::runtime_error("WAV export read failed");
        out.append(pcm);remaining-=n;
    }
    out.close();
}
}
