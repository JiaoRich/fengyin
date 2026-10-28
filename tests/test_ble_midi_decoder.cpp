#include "BleMidiInputDecoder.h"
#include <cassert>
#include <random>
#include <vector>
#include <iostream>
int main(){
 fengyin::BleMidiInputDecoder decoder;std::vector<std::vector<uint8_t>> out;std::vector<uint64_t> time;
 const auto emit=[&](const uint8_t* data,size_t size,uint64_t tick){assert(size>0&&size<=3);out.emplace_back(data,data+size);time.push_back(tick);};
 const uint8_t packet[]{0x80,0x80,0xb0,11,100,0x81,0x90,60,100,0x82,62,90,0x83,0xd0,80,0x84,0xe0,0,64,0x85,0x80,60,0};
 assert(decoder.push(packet,sizeof(packet),1000000,emit));
 assert((out==std::vector<std::vector<uint8_t>>{{0xb0,11,100},{0x90,60,100},{0x90,62,90},{0xd0,80},{0xe0,0,64},{0x80,60,0}}));
 for(size_t i=1;i<time.size();++i)assert(time[i]>=time[i-1]);assert(time.back()<=1000000);
 decoder.reset();out.clear();time.clear();
 const uint8_t running[]{0x80,0x80,0xb0,11,1,11,2,11,3};assert(decoder.push(running,sizeof(running),1100000,emit));assert(out.size()==3&&out.back()[2]==3);
 const uint8_t invalid[]{0,1,2};assert(!decoder.push(invalid,sizeof(invalid),1200000,emit));
 const uint8_t sysexA[]{0x80,0x81,0xf0,1,2,3};const uint8_t sysexB[]{0x80,4,5,0x82,0xf7,0x83,0x90,65,100};
 decoder.reset();out.clear();time.clear();assert(decoder.push(sysexA,sizeof(sysexA),1300000,emit));assert(out.empty());assert(decoder.push(sysexB,sizeof(sysexB),1400000,emit));assert(out.size()==1&&out[0][0]==0x90);
 // Native codec roundtrip across all performance statuses, timestamp wraps and MTU boundaries.
 std::mt19937 rng(136);for(int cycle=0;cycle<2000;++cycle){
  decoder.reset();out.clear();time.clear();MidiBleMidi1::PacketBuilder writer;writer.SetMaxPacketByteCount(20);
  std::vector<std::vector<uint8_t>> expected;
  for(int n=0;n<12;++n){uint8_t status=uint8_t(0x80+(rng()%7)*16+(rng()%16));size_t size=MidiBleMidi1::ExpectedDataByteCount(status)+1;std::vector<uint8_t> message{status,uint8_t(rng()%128),uint8_t(rng()%128)};message.resize(size);expected.push_back(message);writer.AppendMessage(message.data(),message.size(),uint16_t((cycle*89+n*3)%8192));}
  writer.EndPacket();uint64_t arrival=2000000;
  for(const auto& data:writer.TakePackets()){assert(decoder.push(data.data(),data.size(),arrival,emit));arrival+=2000;}
  assert(out==expected);for(size_t n=1;n<time.size();++n)assert(time[n]>=time[n-1]);
 }
 std::cout<<"BLE MIDI: CC11, note on/off, pressure, bend, running status, SysEx skip, clock wrap and 24000-message roundtrip passed\n";
}
