#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <vector>

// Seekable PCM WAV, shared by system-output recording and its native selftest.
class PcmWaveWriter {
    std::fstream file;
    uint32_t bytes=0;
    void u16(uint16_t v){char b[]{char(v),char(v>>8)};file.write(b,2);}
    void u32(uint32_t v){char b[]{char(v),char(v>>8),char(v>>16),char(v>>24)};file.write(b,4);}
    void header(){
        file.seekp(0);file.write("RIFF",4);u32(36+bytes);file.write("WAVEfmt ",8);
        u32(16);u16(1);u16(2);u32(48000);u32(192000);u16(4);u16(16);
        file.write("data",4);u32(bytes);
    }
public:
    explicit PcmWaveWriter(const std::filesystem::path& path) {
        file.open(path,std::ios::binary|std::ios::in|std::ios::out|std::ios::trunc);
        if(!file)throw std::runtime_error("Cannot create system-output WAV");
        header();
    }
    void append(const std::vector<int16_t>& pcm) {
        const auto size=pcm.size()*sizeof(int16_t);
        if(pcm.size()%2 || size>UINT32_MAX-36ULL-bytes)throw std::runtime_error("WAV size limit reached; recording stopped");
        file.write(reinterpret_cast<const char*>(pcm.data()),static_cast<std::streamsize>(size));
        if(!file)throw std::runtime_error("Cannot write system-output WAV");
        bytes+=static_cast<uint32_t>(size);
    }
    void close(){if(file.is_open()){header();file.flush();if(!file)throw std::runtime_error("Cannot finalise WAV");file.close();}}
    ~PcmWaveWriter(){try{close();}catch(...){}}
};
