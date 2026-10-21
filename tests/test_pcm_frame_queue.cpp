#include "../native/ScreenCapture/PcmFrameQueue.h"
#include "../native/ScreenRecorder/PcmWaveWriter.h"
#include <thread>
#include <chrono>
#include <iostream>
using fengyin::screen::PcmFrameQueue;
void require(bool value){if(!value)throw std::runtime_error("PCM queue regression");}
std::int16_t value(std::uint64_t sample){return std::int16_t(int((sample*17)%30001)-15000);}
void fill(std::vector<std::int16_t>& block,std::uint64_t frame){
    for(std::size_t i=0;i<block.size();++i)block[i]=value(frame*2+i);
}
int main(int argc,char** argv){
    if(argc==2&&std::string(argv[1])=="--realtime-30min"){
        PcmFrameQueue queue;constexpr std::uint64_t target=48000ULL*1800;
        std::atomic<bool> overflow{false};const auto origin=std::chrono::steady_clock::now();
        const auto path=std::filesystem::temp_directory_path()/("fy-soak-"+std::to_string(origin.time_since_epoch().count())+".wav");
        std::thread producer([&]{std::vector<std::int16_t> block(256);
            for(std::uint64_t sent=0;sent<target;sent+=128){
                std::this_thread::sleep_until(origin+std::chrono::nanoseconds(sent*1000000000/48000));
                fill(block,sent);if(!queue.push(block.data(),128)){overflow=true;break;}
            }
        });
        std::uint64_t received=0;std::vector<std::int16_t> batch(9600);
        {PcmWaveWriter writer(path);
            while(received<target&&!overflow.load()){
                if(queue.available()<4800){std::this_thread::sleep_for(std::chrono::milliseconds(3));continue;}
                batch.resize(9600);const auto count=queue.pop(batch.data(),4800);batch.resize(count*2);
                for(std::size_t i=0;i<count*2;++i)require(batch[i]==value(received*2+i));
                writer.append(batch);received+=count;
            }writer.close();
        }
        producer.join();require(!overflow&&received==target);
        require(std::filesystem::file_size(path)==44+target*4);std::filesystem::remove(path);
        std::cout<<"REALTIME 30-minute PCM queue+disk PASS, frames="<<received<<", highWater="<<queue.highWaterFrames()<<"\n";return 0;
    }
    // Packet sizes must not determine buffering duration. Includes 1-frame
    // packets and real low-latency sizes that exhausted the old 64-packet queue.
    for(std::size_t packet:{1,64,128,256,480,2048}){
        PcmFrameQueue queue;std::vector<std::int16_t> block(packet*2),out(9600);
        std::uint64_t written=0,read=0;
        while(written<480000){
            block.resize(std::min<std::uint64_t>(packet,480000-written)*2);fill(block,written);
            require(queue.push(block.data(),block.size()/2));written+=block.size()/2;
        }
        require(queue.highWaterFrames()==480000);require(!queue.push(block.data(),1));
        while(auto frames=queue.pop(out.data(),4800)){
            for(std::size_t i=0;i<frames*2;++i)require(out[i]==value(read*2+i));read+=frames;
        }
        require(read==written&&queue.available()==0);
        // Wrap and silence, including a packet larger than the old 2048 limit.
        require(queue.push(nullptr,5000,true));out.resize(10000);
        require(queue.pop(out.data(),5000)==5000);
        for(auto sample:out)require(sample==0);
    }
    // Concurrent wrapping plus exact WAV roundtrip; consumer intentionally
    // stalls before draining. No data loss or reordered/duplicated samples.
    PcmFrameQueue queue;constexpr std::uint64_t frames=48000*30;
    const auto path=std::filesystem::temp_directory_path()/
        ("fy-pcm-queue-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".wav");
    std::thread producer([&]{
        std::vector<std::int16_t> block(256);std::uint64_t sent=0;
        while(sent<frames){fill(block,sent);if(queue.push(block.data(),128))sent+=128;else std::this_thread::yield();}
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(350));
    {
        PcmWaveWriter writer(path);std::vector<std::int16_t> batch(9600);std::uint64_t received=0;
        while(received<frames){
            batch.resize(9600);const auto count=queue.pop(batch.data(),4800);
            if(!count){std::this_thread::yield();continue;}
            for(std::size_t i=0;i<count*2;++i)require(batch[i]==value(received*2+i));
            batch.resize(count*2);writer.append(batch);received+=count;
        }
        writer.close();
    }
    producer.join();require(queue.available()==0);
    require(std::filesystem::file_size(path)==44+frames*4);
    std::ifstream wav(path,std::ios::binary);wav.seekg(44);
    for(std::uint64_t i=0;i<frames*2;++i){std::int16_t sample=0;wav.read(reinterpret_cast<char*>(&sample),2);require(sample==value(i));}
    require(bool(wav));wav.close();std::filesystem::remove(path);
    std::cout<<"PCM: fixed 10-second capacity, 6 packet sizes, wrap/silence, concurrent exact 30-second WAV passed\n";
}
