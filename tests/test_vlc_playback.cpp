#define NOMINMAX
#include <windows.h>
#include "VlcPlaybackEngine.h"
#include "VlcNativeWindow.h"
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <functional>
#include <algorithm>
#include <fstream>

// Evidence only: capture this test's client area, never the whole desktop.
// VLC 3 input statistics are periodically copied while demuxing; a paused
// picture and a zero statistics counter are not sufficient to distinguish
// an actual blank surface from a stale counter. Keep the existing assertion
// until the pixels and raw state establish which case occurred.
bool savePreviewEvidence(HWND window,const std::filesystem::path& path) {
    RECT bounds{}; POINT origin{};
    if(!GetClientRect(window,&bounds) || !ClientToScreen(window,&origin))return false;
    const int width=bounds.right,height=bounds.bottom;
    if(width<=0 || height<=0)return false;
    BITMAPINFO info{};
    info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    info.bmiHeader.biCompression=BI_RGB;
    const auto screen=GetDC(nullptr);if(!screen)return false;
    const auto dc=CreateCompatibleDC(screen);
    void* data=nullptr;
    const auto bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&data,nullptr,0);
    if(!dc || !bitmap || !data) {
        if(bitmap)DeleteObject(bitmap);if(dc)DeleteDC(dc);ReleaseDC(nullptr,screen);
        std::cout<<"PREVIEW capture unavailable\n";return false;
    }
    const auto previous=SelectObject(dc,bitmap);
    const bool captured=BitBlt(dc,0,0,width,height,screen,origin.x,origin.y,SRCCOPY|CAPTUREBLT)!=0;
    GdiFlush();
    bool picture=false;
    if(captured) {
        const auto bytes=static_cast<DWORD>(width*height*4);
        BITMAPFILEHEADER header{};header.bfType=0x4d42;
        header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+bytes;
        if(!path.empty()) {
            std::ofstream file(path,std::ios::binary);
            file.write(reinterpret_cast<const char*>(&header),sizeof(header));
            file.write(reinterpret_cast<const char*>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));
            file.write(static_cast<const char*>(data),bytes);
        }
        const auto* pixels=static_cast<const unsigned char*>(data);
        unsigned red=0,blue=0;
        for(int y=0;y<height;++y)for(int x=0;x<width;++x) {
            const auto* p=pixels+(y*width+x)*4;
            if(y<height/2 && p[2]>150 && p[1]<100 && p[0]<100)++red;
            if(y>=height/2 && p[0]>150 && p[1]<100 && p[2]<100)++blue;
        }
        picture=red>unsigned(width*height/8) && blue>unsigned(width*height/8);
        if(!path.empty())std::cout<<"PREVIEW pixels width="<<width<<" height="<<height
                 <<" redTop="<<red<<" blueBottom="<<blue<<" picture="<<picture<<"\n";
    } else std::cout<<"PREVIEW capture failed win32="<<GetLastError()<<"\n";
    SelectObject(dc,previous);DeleteObject(bitmap);DeleteDC(dc);ReleaseDC(nullptr,screen);
    return picture;
}

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
        WS_OVERLAPPEDWINDOW|WS_VISIBLE|WS_CLIPCHILDREN|SS_BLACKRECT,30,30,640,400,nullptr,nullptr,GetModuleHandleW(nullptr),nullptr);
    if(!window)return 2;
    const auto button=CreateWindowExW(0,L"BUTTON",L"Underlying FengYin controls",
        WS_CHILD|WS_VISIBLE,0,0,624,361,window,nullptr,GetModuleHandleW(nullptr),nullptr);
    const auto surface=fengyin::createVlcChildWindow(window);
    if(!button || !surface)return 2;
    MoveWindow(surface,0,0,624,361,FALSE);
    int code=0;
    try {
        const POINT centre{200,150};
        for(int i=0;i<20;++i) {
            if(!fengyin::setVlcChildVisible(surface,window,true) || GetParent(surface)!=window
                || (GetWindowLongPtrW(surface,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0)
                throw std::runtime_error("VLC surface escaped its non-topmost child window");
            fengyin::setVlcChildVisible(surface,window,false);
            if(IsWindowVisible(surface) || ChildWindowFromPointEx(window,centre,CWP_SKIPINVISIBLE)!=button)
                throw std::runtime_error("Hidden VLC surface intercepts underlying controls");
        }
        ShowWindow(window,SW_MINIMIZE);pump();
        fengyin::setVlcChildVisible(surface,window,true);
        if(GetWindowLongPtrW(surface,GWL_STYLE)&WS_VISIBLE)throw std::runtime_error("Minimised parent re-showed video");
        ShowWindow(window,SW_RESTORE);pump();
        if(IsWindowVisible(surface))throw std::runtime_error("Restore resurrected stale video");
        if(!fengyin::setVlcChildVisible(surface,window,true))throw std::runtime_error("Fresh viewport failed");
        std::cout<<"PASS native video child ownership, 20 show/hide hit tests and minimise/restore\n";
        VlcPlaybackEngine p(std::filesystem::path(argv[1]),surface);Shutdown cleanup{p};
        // Exercise the exact failed-188 input: forward slash Windows absolute
        // path, with Unicode and a space as ordinary user media names contain.
        const auto fixture=std::filesystem::path(argv[2]);
        const auto unicode=fixture.parent_path()/L"风吟 VLC 回归.mp4";
        std::filesystem::copy_file(fixture,unicode,std::filesystem::copy_options::overwrite_existing);
        const auto bytes=unicode.u8string();
        std::string file(reinterpret_cast<const char*>(bytes.data()),bytes.size());
        std::replace(file.begin(),file.end(),'\\','/');
        p.load(file,1);
        auto lastPreviewLog=GetTickCount64();
        auto lastPreviewCapture=lastPreviewLog;
        bool previewPicture=false;
        auto previewState=[&]{const auto s=p.getState();
            std::cout<<"PREVIEW rawState="<<p.testPlaybackState.load()<<" ready="<<s.ready
                     <<" playing="<<s.playing<<" position="<<s.position<<" duration="<<s.duration
                     <<" decoded="<<s.decodedVideo<<" displayed="<<s.displayedVideo
                     <<" pcmSamples="<<p.testPcmSamples.load()<<" error="<<s.error<<std::endl;};
        try {
            waitFor([&]{auto s=p.getState();
                if(GetTickCount64()-lastPreviewLog>=1000){lastPreviewLog=GetTickCount64();previewState();}
                if(!s.error.empty())throw std::runtime_error(s.error);
                // A paused input can leave VLC's demux statistics unchanged.
                // Require the actual red/blue fixture on the native surface.
                if(s.ready && s.duration>10 && GetTickCount64()-lastPreviewCapture>=100) {
                    lastPreviewCapture=GetTickCount64();previewPicture=savePreviewEvidence(surface,{});
                }
                return previewPicture;
            },15000,"paused first frame not ready");
        } catch(...) {
            previewState();savePreviewEvidence(surface,fixture.parent_path()/L"vlc-first-frame-failed.bmp");throw;
        }
        previewState();savePreviewEvidence(surface,fixture.parent_path()/L"vlc-first-frame.bmp");
        const auto idleEnd=GetTickCount64()+700;while(GetTickCount64()<idleEnd)pump();
        if(p.getState().playing || p.getState().position>.15 || p.testPcmSamples>0)
            throw std::runtime_error("First-frame preview advanced playback or emitted audio");
        const auto stablePreview=p.getState().decodedVideo;
        const auto previewEnd=GetTickCount64()+700;while(GetTickCount64()<previewEnd)pump();
        if(p.getState().decodedVideo!=stablePreview)throw std::runtime_error("Paused preview kept decoding");
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
        const auto errorEnd=GetTickCount64()+600;while(GetTickCount64()<errorEnd)pump();
        if(p.getState().error.empty() || p.getState().finished || p.getState().ready)
            throw std::runtime_error("Transient error was lost or misreported as EOF");
        p.load(file,11);p.setPlaying(true);
        waitFor([&]{auto s=p.getState();return s.generation==11 && s.error.empty() && s.position>.2;},6000,"Error recovery failed");
        const auto corrupt=fixture.parent_path()/L"corrupt-media.mp4";
        {std::ofstream bad(corrupt,std::ios::binary);bad<<"not a media file";}
        const auto corruptBytes=corrupt.u8string();
        p.load(std::string(reinterpret_cast<const char*>(corruptBytes.data()),corruptBytes.size()),12);p.setPlaying(true);
        waitFor([&]{return !p.getState().error.empty();},6000,"Corrupt media error missing");
        p.load(file,13);p.setPlaying(true);
        waitFor([&]{auto s=p.getState();return s.generation==13 && s.error.empty() && s.position>.2;},6000,"Corrupt media recovery failed");
        // A real silent audio-only file must not be mistaken for invalid media.
        const auto silent=fixture.parent_path()/L"silent-audio.wav";
        {std::ofstream wav(silent,std::ios::binary);
            auto le=[&](unsigned v,int n){for(int i=0;i<n;++i)wav.put(static_cast<char>(v>>(i*8)));};
            wav.write("RIFF",4);le(16036,4);wav.write("WAVEfmt ",8);le(16,4);le(1,2);le(1,2);
            le(8000,4);le(16000,4);le(2,2);le(16,2);wav.write("data",4);le(16000,4);
            for(int i=0;i<16000;++i)wav.put(0);
        }
        const auto silentBytes=silent.u8string();
        p.load(std::string(reinterpret_cast<const char*>(silentBytes.data()),silentBytes.size()),14);p.setPlaying(true);
        waitFor([&]{auto s=p.getState();if(!s.error.empty())throw std::runtime_error(s.error);return s.generation==14 && s.finished;},6000,"Silent audio-only EOF missing");
        p.load(file,15);p.setPlaying(true);
        waitFor([&]{auto s=p.getState();return s.generation==15 && s.error.empty() && s.position>.2;},6000,"Audio-only recovery failed");
        std::cout<<"PASS silent first-frame preview, pause, explicit/stale seek, EOF replay, replace, unload, failure recovery; shutdown pending\n";
    } catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<"\n";code=1;}
    DestroyWindow(window);
    if(!code)std::cout<<"PASS shutdown. Decoded PCM sink is CI-only; physical speakers/SWAM load remain hardware acceptance.\n";
    return code;
}
