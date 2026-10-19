// Diagnostic only: real WebView2 + production VLC surface/engine. No instrument,
// microphone, user settings, recording, or customer's media is opened.
#define NOMINMAX
#include <juce_gui_extra/juce_gui_extra.h>
#include <windows.h>
#include <dbghelp.h>
#include "VlcVideoSurface.h"
#include "VideoHostRendering.h"
#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>

static uint64_t now() { return GetTickCount64(); }
static void dumpProcess(const wchar_t* filename) {
    auto file=CreateFileW(filename,GENERIC_WRITE,0,nullptr,CREATE_NEW,0,nullptr);
    if(file!=INVALID_HANDLE_VALUE){
        const bool ok=MiniDumpWriteDump(GetCurrentProcess(),GetCurrentProcessId(),file,
            MiniDumpNormal,nullptr,nullptr,nullptr)!=FALSE;
        CloseHandle(file);std::cerr<<"dump saved="<<ok<<std::endl;
    }
}
static constexpr auto html=R"HTML(<!doctype html><meta charset="utf-8">
<style>body{margin:0;background:#123;color:white;font:18px sans-serif}header{height:60px}
button{height:45px;width:130px}#surface{position:fixed;left:20px;top:70px;width:480px;height:270px}
body.expanded #surface{left:10px;top:60px;width:calc(100vw - 20px);height:calc(100vh - 140px)}</style>
<header><button id="expand">Expand</button><button id="check">Check input</button></header>
<div id="surface"></div><script>
const send=(name,payload)=>window.__JUCE__.backend.emitEvent(name,payload);
let count=0;function report(){let r=document.querySelector('#surface').getBoundingClientRect();
send('viewport',{x:r.x,y:r.y,width:r.width,height:r.height,visible:true});}
document.querySelector('#expand').onclick=()=>{send('viewport',{visible:false});
document.body.classList.toggle('expanded');requestAnimationFrame(()=>{report();send('expanded',{});});};
document.querySelector('#check').onclick=()=>send('clicked',{count:++count});
setInterval(()=>send('heartbeat',{}),100);window.addEventListener('resize',report);
send('ready',{});report();</script>)HTML";

struct Scene final:juce::Component,juce::Timer {
    fengyin::VlcVideoSurface surface;
    std::unique_ptr<juce::WebBrowserComponent> web;
    juce::File fixture;
    bool deferred=false,attachBeforePlayer=false,webOnly=false,ready=false,expandedAck=false,failed=false;
    int clickAck=0,cycle=0,phase=0,generation=0;
    uint64_t deadline=0,lastPictureLog=0;
    std::atomic<uint64_t> ui{now()},javascript{now()};
    explicit Scene(juce::File input,const juce::String& mode):fixture(input),
        deferred(mode.contains("deferred")),attachBeforePlayer(mode.startsWith("attached-")),
        webOnly(mode=="web-only") {
        setOpaque(true);
        auto options=juce::WebBrowserComponent::Options{}
          .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
          .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2{}
            .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory)
              .getChildFile("fy-vlc-webview-"+juce::Uuid().toString())))
          .withNativeIntegrationEnabled()
          .withResourceProvider([](const juce::String&)->std::optional<juce::WebBrowserComponent::Resource>{
              const std::string source=html;std::vector<std::byte> bytes(source.size());
              std::memcpy(bytes.data(),source.data(),source.size());
              return juce::WebBrowserComponent::Resource{std::move(bytes),"text/html"};})
          .withEventListener("heartbeat",[this](juce::var){javascript.store(now());})
          .withEventListener("ready",[this](juce::var){
              // Cold WebView startup is not a post-ready heartbeat failure.
              javascript.store(now());ready=true;std::cout<<"web ready\n"<<std::flush;})
          .withEventListener("expanded",[this](juce::var){expandedAck=true;})
          .withEventListener("clicked",[this](juce::var p){clickAck=int(p.getProperty("count",0));})
          .withEventListener("viewport",[this](juce::var p){
              if(webOnly)return;
              const auto apply=[safe=juce::Component::SafePointer<Scene>(this),p]{if(safe){
                  const juce::Rectangle<int> bounds(int(p.getProperty("x",0)),int(p.getProperty("y",0)),
                    int(p.getProperty("width",0)),int(p.getProperty("height",0)));
                  std::cout<<"viewport enter visible="<<bool(p.getProperty("visible",false))
                    <<" bounds="<<bounds.toString()<<" hwnd="<<safe->surface.getHWND()<<std::endl;
                  safe->surface.updateViewport(bounds,bool(p.getProperty("visible",false)));
                  std::cout<<"viewport returned\n"<<std::flush;}};
              if(deferred)juce::MessageManager::callAsync(apply);else apply();});
        web=std::make_unique<juce::WebBrowserComponent>(options);
        addAndMakeVisible(*web);addAndMakeVisible(surface);
        setSize(900,650);addToDesktop(juce::ComponentPeer::windowHasTitleBar);
        if(!fengyin::configureVideoHostRendering(getPeer())) {
            failed=true;std::cerr<<"FAIL production backing renderer unavailable\n";
        }
        setTopLeftPosition(80,80);setVisible(true);toFront(true);
        web->setBounds(getLocalBounds());
        web->goToURL(juce::WebBrowserComponent::getResourceProviderRoot());
        deadline=now()+30000;startTimer(20);
    }
    ~Scene()override{stopTimer();surface.shutdown();web.reset();}
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff07101d));}
    void finish(){stopTimer();juce::MessageManager::getInstance()->stopDispatchLoop();}
    void captureFailure()const {
        const auto hwnd=static_cast<HWND>(getPeer()->getNativeHandle());RECT r{};
        if(!GetWindowRect(hwnd,&r))return;
        const int width=r.right-r.left,height=r.bottom-r.top;
        if(width<=0||height<=0)return;
        auto screen=GetDC(nullptr);if(!screen)return;
        auto memory=CreateCompatibleDC(screen);
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
        info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        void* pixels=nullptr;auto bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
        if(memory&&bitmap&&pixels){
            auto old=SelectObject(memory,bitmap);
            const auto copied=BitBlt(memory,0,0,width,height,screen,r.left,r.top,SRCCOPY|CAPTUREBLT);
            if(copied){
                GdiFlush();
                BITMAPFILEHEADER header{};header.bfType=0x4d42;
                header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
                header.bfSize=header.bfOffBits+width*height*4;
                auto file=CreateFileW(L"failed-window.bmp",GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,0,nullptr);
                if(file!=INVALID_HANDLE_VALUE){DWORD written=0;
                    WriteFile(file,&header,sizeof(header),&written,nullptr);
                    WriteFile(file,&info.bmiHeader,sizeof(info.bmiHeader),&written,nullptr);
                    WriteFile(file,pixels,width*height*4,&written,nullptr);CloseHandle(file);}
            }
            SelectObject(memory,old);
        }
        if(bitmap)DeleteObject(bitmap);if(memory)DeleteDC(memory);ReleaseDC(nullptr,screen);
    }
    bool firstPictureVisible() {
        const auto hwnd=static_cast<HWND>(surface.getHWND());RECT bounds{};
        if(!hwnd||!IsWindowVisible(hwnd)||!GetClientRect(hwnd,&bounds)
            ||bounds.right<8||bounds.bottom<8)return false;
        // Release fixtures have red above blue; standalone fixtures are red.
        // Sample the known red region, never the ambiguous centre boundary.
        POINT point{bounds.right/2,bounds.bottom/4};ClientToScreen(hwnd,&point);
        const auto screen=GetDC(nullptr);if(!screen)return false;
        const auto colour=GetPixel(screen,point.x,point.y);ReleaseDC(nullptr,screen);
        if(now()-lastPictureLog>1000){
            lastPictureLog=now();const auto state=surface.getState();
            std::cout<<"picture rgb="<<int(GetRValue(colour))<<","<<int(GetGValue(colour))<<","<<int(GetBValue(colour))
                <<" client="<<bounds.right<<"x"<<bounds.bottom<<" screen="<<point.x<<","<<point.y
                <<" hit="<<WindowFromPoint(point)<<" hwnd="<<hwnd<<" parent="<<GetParent(hwnd)
                <<" foreground="<<GetForegroundWindow()<<" decoded="<<state.decodedVideo
                <<" displayed="<<state.displayedVideo<<" lost="<<state.lostVideo
                <<" state="<<surface.player()->testPlaybackState.load()<<std::endl;
        }
        // The owned fixture is red; a blank HWND or the blue web page cannot pass.
        return colour!=CLR_INVALID&&GetRValue(colour)>150&&GetGValue(colour)<100&&GetBValue(colour)<100;
    }
    void click(int x) {
        auto* peer=getPeer();const auto scale=peer->getPlatformScaleFactor();
        POINT p{juce::roundToInt(x*scale),juce::roundToInt(25*scale)};
        ClientToScreen(static_cast<HWND>(peer->getNativeHandle()),&p);
        const auto hit=WindowFromPoint(p);
        wchar_t cls[128]{};GetClassNameW(hit,cls,128);
        std::cout<<"input cycle="<<cycle<<" phase="<<phase<<" hit="<<juce::String(cls)
          <<" foreground="<<GetForegroundWindow()<<" focus="<<GetFocus()<<std::endl;
        SetCursorPos(p.x,p.y);INPUT inputs[2]{};
        inputs[0].type=inputs[1].type=INPUT_MOUSE;
        inputs[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;inputs[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        if(SendInput(2,inputs,sizeof(INPUT))!=2){failed=true;std::cerr<<"SendInput failed\n";}
    }
    void timerCallback()override {
        ui.store(now());
        if(failed){finish();return;}
        if(ready&&now()-javascript.load()>5000){
            failed=true;std::cerr<<"FAIL web heartbeat stalled nativeAgeMs="<<(now()-ui.load())
                <<" phase="<<phase<<std::endl;captureFailure();dumpProcess(L"web-stall.dmp");finish();return;
        }
        if(now()>deadline){failed=true;std::cerr<<"FAIL stage timeout phase="<<phase<<std::endl;
            captureFailure();dumpProcess(L"stage-timeout.dmp");finish();return;}
        if(phase==0&&ready){
            if(webOnly){phase=2;deadline=now()+10000;return;}
            // Test the discovered attachment ordering independently of callback
            // deferral. Production initialise() itself is not changed here.
            if(attachBeforePlayer)surface.setVisible(true);
            std::cout<<"initialise enter attached="<<attachBeforePlayer<<std::endl;
            if(!surface.initialise()){failed=true;finish();return;}
            std::cout<<"initialise returned hwnd="<<surface.getHWND()<<std::endl;
            surface.player()->load(fixture.getFullPathName().toStdString(),++generation);
            phase=1;deadline=now()+30000;
        } else if(phase==1&&surface.getState().generation==generation&&surface.getState().ready){
            web->evaluateJavascript("report()");
            phase=5;deadline=now()+10000;
        } else if(phase==5&&firstPictureVisible()){
            const auto pcm=surface.player()->testPcmNonzero.load();
            if(generation==1&&pcm!=0){failed=true;finish();std::cerr<<"FAIL preview emitted accompaniment\n";return;}
            std::cout<<"PASS real paused first picture generation="<<generation<<" cumulativePCM="<<pcm<<std::endl;
            surface.player()->setPlaying(true);phase=2;deadline=now()+10000;
        } else if(phase==2&&(webOnly||(surface.getState().generation==generation&&surface.getState().displayedVideo>0))){
            expandedAck=false;phase=3;deadline=now()+5000;click(60);
        } else if(phase==3&&expandedAck){
            phase=4;deadline=now()+5000;click(195);
        } else if(phase==4&&clickAck>cycle){
            std::cout<<"PASS physical click cycle="<<cycle<<" frames="<<surface.getState().displayedVideo
              <<" nonzeroPCM="<<(webOnly?0:surface.player()->testPcmNonzero.load())<<std::endl;
            if(++cycle==20){finish();return;}
            if(!webOnly&&cycle%5==0){surface.player()->load(fixture.getFullPathName().toStdString(),++generation);phase=1;deadline=now()+30000;}
            else {phase=2;deadline=now()+10000;}
        }
    }
};

#include "ProductionUiCheck.h"

template<class CheckedScene>int runCheckedScene(CheckedScene& scene) {
    const DWORD uiThread=GetCurrentThreadId();std::atomic<bool> done{false};
    std::thread watchdog([&]{while(!done.load()){
        if(now()-scene.ui.load()>5000){
            GUITHREADINFO info{sizeof(info)};GetGUIThreadInfo(uiThread,&info);
            std::cerr<<"FAIL native heartbeat stalled focus="<<info.hwndFocus<<" capture="<<info.hwndCapture
              <<" active="<<info.hwndActive<<" flags="<<info.flags<<std::endl;
            dumpProcess(L"native-ui-stall.dmp");
            ExitProcess(3);
        }Sleep(200);
    }});
    juce::MessageManager::getInstance()->runDispatchLoop();
    if(!scene.failed && !scene.webOnly && scene.surface.player()->testPcmNonzero.load()==0){
        std::cerr<<"FAIL no actual decoded nonzero accompaniment PCM\n";scene.failed=true;
    }
    done.store(true);watchdog.join();
    return scene.failed?1:0;
}

int main(int argc,char** argv) {
    if(argc!=3){std::cerr<<"red-fixture.mp4 web-only|immediate|deferred|attached-immediate|attached-deferred|production-ui\n";return 2;}
    juce::ScopedJuceInitialiser_GUI init;
    const auto fixture=juce::File(juce::String::fromUTF8(argv[1]));
    if(!fixture.existsAsFile())return 2;
    if(juce::String(argv[2])=="production-ui") {
        ProductionUiScene scene(fixture);return runCheckedScene(scene);
    }
    Scene scene(fixture,juce::String(argv[2]));return runCheckedScene(scene);
}
