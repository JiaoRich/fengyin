#define NOMINMAX
#include <windows.h>
#include "VlcPlaybackEngine.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <functional>

using fengyin::VlcPlaybackEngine;
void pump() { MSG m{}; while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageW(&m);} Sleep(5); }
void waitFor(const std::function<bool()>& predicate,int milliseconds,const char* label) {
    const auto end=GetTickCount64()+milliseconds;
    while(GetTickCount64()<end){pump();if(predicate())return;}
    throw std::runtime_error(label);
}
struct Shutdown {
    VlcPlaybackEngine& player;
    ~Shutdown(){player.requestShutdown();const auto end=GetTickCount64()+15000;
        while(!player.hasStopped() && GetTickCount64()<end)pump();
        if(!player.hasStopped()){std::cerr<<"FAIL VLC shutdown timeout\n";ExitProcess(2);}}
};
int wmain(int argc,wchar_t** argv) {
    if(argc!=3){std::cerr<<"Expected runtime directory and a 12-second H264/AAC fixture\n";return 2;}
    HWND window=CreateWindowExW(0,L"STATIC",L"FengYin VLC playback integration",
        WS_OVERLAPPEDWINDOW|WS_VISIBLE|SS_BLACKRECT,30,30,640,400,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return 2;
    int code=0;
    try {
        VlcPlaybackEngine p(std::filesystem::path(argv[1]),window);Shutdown cleanup{p};
        const auto bytes=std::filesystem::path(argv[2]).u8string();
        const std::string file(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        p.load(file,1);
        waitFor([&]{auto s=p.getState();if(!s.error.empty())throw std::runtime_error(s.error);return s.ready && s.duration>10;},15000,"metadata not ready");
        const auto before=p.getState();
        const auto idleEnd=GetTickCount64()+700;while(GetTickCount64()<idleEnd)pump();
        if(p.getState().playing || p.getState().decodedVideo>before.decodedVideo || p.testPcmSamples>0)
            throw std::runtime_error("Loading started playback or background decoding");
        p.setVolume(.45f);p.setPlaying(true);
        waitFor([&]{auto s=p.getState();return s.position>.6 && s.decodedVideo>0 && s.displayedVideo>0 && p.testPcmNonzero>1000;},15000,"No decoded video/nonzero PCM");
        std::cout<<"PASS real LibVLC H264 frames="<<p.getState().displayedVideo<<" nonzero PCM="<<p.testPcmNonzero.load()<<"\n";
        p.setPlaying(false);
        waitFor([&]{return !p.getState().playing;},4000,"Pause did not complete");
        const auto pauseEnd=GetTickCount64()+400;while(GetTickCount64()<pauseEnd)pump();
        const auto paused=p.getState().position;
        const auto stableEnd=GetTickCount64()+500;while(GetTickCount64()<stableEnd)pump();
        if(std::abs(p.getState().position-paused)>.15)throw std::runtime_error("Paused clock drift");
        p.seek(4,1);p.setPlaying(true);
        waitFor([&]{auto s=p.getState();return s.seekRevision==1 && s.position>=3.8 && s.position<7;},6000,"Explicit seek failed");
        p.seek(0,0); // stale seek must not win
        p.setPlaying(false);
        waitFor([&]{return !p.getState().playing;},4000,"Second pause failed");
        if(p.getState().seekRevision!=1)throw std::runtime_error("Stale revision won");
        p.seek(11,2);p.setPlaying(true);
        waitFor([&]{return p.getState().finished;},8000,"EOF missing");
        p.seek(0,3);p.setPlaying(true);
        waitFor([&]{auto s=p.getState();return s.playing && !s.finished && s.position>.2 && s.position<4;},6000,"EOF replay failed");
        // Last selection wins; no queued full-file work or stale state publication.
        for(int g=2;g<=8;++g)p.load(file,g);
        waitFor([&]{auto s=p.getState();return s.generation==8 && s.ready && s.duration>10;},10000,"Latest selection did not win");
        if(p.getState().playing)throw std::runtime_error("New selection autoplayed");
        p.setPlaying(true);
        waitFor([&]{return p.getState().position>.2;},6000,"Reloaded media silent");
        p.unload();waitFor([&]{return p.getState().generation==9 && !p.getState().ready;},6000,"Unload failed");
        p.load(file+".missing",10);p.setPlaying(true);
        waitFor([&]{return !p.getState().error.empty();},6000,"Invalid media error missing");
        p.load(file,11);p.setPlaying(true);
        waitFor([&]{auto s=p.getState();return s.generation==11 && s.error.empty() && s.position>.2;},6000,"Error recovery failed");
        std::cout<<"PASS load-without-play, pause, explicit/stale seek, EOF replay, replace, unload, failure recovery; shutdown pending\n";
    } catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";code=1;}
    DestroyWindow(window);
    if(!code)std::cout<<"PASS shutdown. Decoded PCM sink is CI-only; physical speakers/SWAM load remain hardware acceptance.\n";
    return code;
}
