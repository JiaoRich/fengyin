#include "../native/ScreenCapture/ExportWave.h"
#include <chrono>
#include <iostream>
using namespace fengyin::screen;
void require(bool v){if(!v)throw std::runtime_error("Aligned WAV regression");}
int main(){
    const auto root=std::filesystem::temp_directory_path()/("fy-wave-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(root);const auto source=root/"audio.wav",target=root/"recording.wav";
    const std::int64_t seconds=1800,lead=3,total=(seconds+lead)*48000;
    {PcmWaveWriter writer(source);std::vector<std::int16_t> block(9600);
     for(std::int64_t frame=0;frame<total;frame+=4800){
        for(std::size_t n=0;n<block.size();++n)block[n]=std::int16_t(int((frame*2+n)%30001)-15000);
        writer.append(block);
     }writer.close();}
    exportWave(source,target,lead*10000000,seconds*10000000);
    require(std::filesystem::file_size(target)==44+seconds*192000);
    std::ifstream output(target,std::ios::binary);output.seekg(44);std::vector<std::int16_t> data(9600);
    for(std::int64_t frame=0;frame<seconds*48000;frame+=4800){
        output.read(reinterpret_cast<char*>(data.data()),19200);require(bool(output));
        for(std::size_t n=0;n<data.size();++n)require(data[n]==std::int16_t(int(((frame+lead*48000)*2+n)%30001)-15000));
    }output.close();
    bool rejected=false;try{exportWave(source,target,0,10000000);}catch(...){rejected=true;}require(rejected);
    rejected=false;try{exportWave(source,root/"short.wav",0,(seconds+lead+1)*10000000);}catch(...){rejected=true;}require(rejected);
    require(!std::filesystem::exists(root/"short.wav"));
    std::filesystem::remove_all(root); // Only the unique fixture created above.
    std::cout<<"30-minute WAV: exact duration and every sample verified; cue excluded; collision and missing coverage rejected\n";
}
