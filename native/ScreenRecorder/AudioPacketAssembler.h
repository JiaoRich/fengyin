#pragma once
#include <algorithm>
#include <cstdint>
#include <vector>

// Only a real subsequent packet (or final stop) may prove missing audio.
// Encoder stalls and empty capture polls must never advance this cursor.
class CaptureAudioAssembler {
    int64_t cursor=0;
public:
    int64_t position() const {return cursor;}
    template<class Write> void padTo(int64_t end,Write write) {
        while(cursor<end) {
            const auto count=std::min<int64_t>(end-cursor,4800);
            write(std::vector<int16_t>(static_cast<size_t>(count)*2,0),cursor);
            cursor+=count;
        }
    }
    template<class Write> void packet(int64_t first,std::vector<int16_t> pcm,Write write) {
        padTo(first,write);
        const auto skip=static_cast<size_t>(std::clamp<int64_t>(cursor-first,0,pcm.size()/2));
        if(skip)pcm.erase(pcm.begin(),pcm.begin()+static_cast<ptrdiff_t>(skip*2));
        if(!pcm.empty()){write(pcm,cursor);cursor+=static_cast<int64_t>(pcm.size()/2);}
    }
};
