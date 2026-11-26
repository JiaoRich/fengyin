#include "Sf2Voice.h"
#include <chrono>
#include <iostream>
#include <array>
#include <iomanip>
#if __APPLE__
#include <dlfcn.h>
#endif

// Optional user-authorized public banks, not downloaded or shipped by CI.
int main(int argc,char** argv)
{
    if(argc<2)return 2;
    int failures=0;
    void (*rtBegin)()=nullptr;unsigned long long (*rtEnd)()=nullptr;
#if __APPLE__
    rtBegin=reinterpret_cast<void(*)()>(dlsym(RTLD_DEFAULT,"fy_sf2_rt_begin"));
    rtEnd=reinterpret_cast<unsigned long long(*)()>(dlsym(RTLD_DEFAULT,"fy_sf2_rt_end"));
#endif
    auto begin=[&]{if(rtBegin)rtBegin();};auto end=[&]{return rtEnd?rtEnd():0;};

    for(int file=1;file<argc;++file)try {
        fengyin::sf2::Voice voice(48000);
        const auto presets=voice.load(argv[file]);
        const auto allocationsBefore=end();
        int audible=0;double peak=0;std::vector<double> costs;
        std::array<float,128> l{},r{};
        auto midi=[&](int s,int a,int b){const unsigned char d[]{(unsigned char)s,(unsigned char)a,(unsigned char)b};begin();voice.midi(d,3);end();};
        auto render=[&](int blocks){double energy=0;for(int block=0;block<blocks;++block){
            auto start=std::chrono::steady_clock::now();begin();voice.render(l.data(),r.data(),128);end();
            costs.push_back(std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count());
            for(int i=0;i<128;++i){if(!std::isfinite(l[i])||!std::isfinite(r[i]))throw std::runtime_error("Nonfinite output");
                peak=std::max(peak,double(std::max(std::abs(l[i]),std::abs(r[i]))));energy+=l[i]*l[i]+r[i]*r[i];}}
            return energy;};
        for(const auto& p:presets){
            if(!voice.select(p.bank,p.program))throw std::runtime_error("Preset selection failed");
            midi(0xb0,11,100);midi(0x90,60,90);midi(0x90,69,90);
            if(render(200)>1e-8)++audible;
            // The callback's CC11=0 must stop expression even with voices held.
            midi(0xb0,11,0);render(200);
            if(render(60)>1e-8){++failures;std::cerr<<"CC11 zero not silent: "<<p.bank<<":"<<p.program<<" "<<p.name<<"\n";}
            midi(0xb0,120,0);if(render(60)>1e-8)throw std::runtime_error("AllSoundOff failed");
        }
        const auto allocations=end()-allocationsBefore;
        if(rtBegin && allocations){++failures;std::cerr<<"Realtime allocator calls="<<allocations<<"\n";}
        std::sort(costs.begin(),costs.end());
        if(!audible)throw std::runtime_error("No audible preset");
        std::cout<<std::setprecision(6)<<"bank="<<argv[file]<<" presets="<<presets.size()<<" audible-at-C4/A4="<<audible<<" peak="<<peak
            <<" callback-us-p99.9="<<costs[size_t(double(costs.size()-1)*.999)]<<" max="<<costs.back()<<" allocator-instrumented="<<bool(rtBegin)<<" allocator-calls="<<allocations<<"\n";
    }catch(const std::exception& e){++failures;std::cerr<<argv[file]<<": "<<e.what()<<"\n";}
    return failures?1:0;
}
