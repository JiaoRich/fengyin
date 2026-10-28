#pragma once
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <utility>
#include <vector>
#if defined(_WIN32)
#include <sal.h>
#else
#define _In_
#define _Inout_
#define _In_reads_bytes_(n)
#define _In_reads_bytes_opt_(n)
#endif
#include "../../third_party/ble-midi/midi_ble_midi1_codec.h"
#if !defined(_WIN32)
#undef _In_
#undef _Inout_
#undef _In_reads_bytes_
#undef _In_reads_bytes_opt_
#endif
namespace fengyin {
// Reuse Microsoft's MIT-licensed packet decoder and sender-clock correlator.
// Only complete short performance messages are emitted. SysEx is skipped with
// bounded state; notification data never enters the real-time audio thread.
class BleMidiInputDecoder {
public:
    void reset() { decoder.Reset();clock.Reset();skipSysex=false;segments.clear();timestamps.clear(); }
    template<class Emit> bool push(const uint8_t* bytes,size_t count,uint64_t arrivalMicroseconds,Emit&& emit) {
        if(count>512)return false;
        if(!decoder.DecodePacket(bytes,count,segments))return false;
        clock.MapPacket(segments,arrivalMicroseconds,1000,timestamps);
        uint8_t message[3] {};size_t used=0,expected=0;
        for(size_t n=0;n<segments.size();++n)for(auto byte:segments[n].Bytes) {
            if(byte>=0xf8){emit(&byte,1,timestamps[n]);continue;}
            if(byte==0xf0){skipSysex=true;used=0;continue;}
            if(byte==0xf7){skipSysex=false;used=0;continue;}
            if(byte&0x80) {
                skipSysex=false;message[0]=byte;used=1;
                expected=size_t(MidiBleMidi1::ExpectedDataByteCount(byte))+1;
                if(expected==1){emit(message,1,timestamps[n]);used=0;}
            } else if(!skipSysex && used>0 && used<3) {
                message[used++]=byte;
                if(used==expected){emit(message,used,timestamps[n]);used=0;}
            }
        }
        return true;
    }
private:
    MidiBleMidi1::PacketDecoder decoder;
    MidiBleMidi1::TimestampCorrelator clock;
    std::vector<MidiBleMidi1::DecodedSegment> segments;
    std::vector<uint64_t> timestamps;
    bool skipSysex=false;
};
}
