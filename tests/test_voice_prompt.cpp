#include "VoicePromptService.h"
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=2)return 1;
    juce::MemoryBlock data;
    if(!juce::File(juce::String::fromUTF8(argv[1])).loadFileAsData(data))return 2;
    fengyin::VoicePromptService voice;
    if(voice.play("bad",3,1,.5f))return 3;
    if(!voice.play(data.getData(),static_cast<int>(data.getSize()),7,.45f))return 4;
    float left[512],right[512];float* outputs[]{left,right};
    float peak=0;int finished=0;
    for(int block=0;block<12000 && !finished;++block){
        voice.audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,512,{});
        for(int i=0;i<512;++i){if(left[i]!=right[i]||!std::isfinite(left[i]))return 5;peak=std::max(peak,std::abs(left[i]));}
        finished=voice.consumeFinished();
    }
    if(finished!=7 || peak<=0 || peak>.451f || voice.consumeFinished()!=0)return 6;
    voice.play(data.getData(),static_cast<int>(data.getSize()),8,.45f);voice.stop();
    voice.audioDeviceIOCallbackWithContext(nullptr,0,outputs,2,512,{});
    for(auto v:left)if(v!=0)return 7;
    std::cout<<"PASS offline decoding, resampling, stereo output, completion and cancellation\n";
}
