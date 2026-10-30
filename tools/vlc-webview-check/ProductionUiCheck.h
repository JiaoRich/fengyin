#pragma once
#include "VideoFileChooser.h"
// Diagnostic only. Real release HTML/CSS/JS, file:// origin, physical input,
// production VLC/host renderer. Fake meter data is NOT an instrument DSP test.
struct ProductionUiScene final:juce::Component,juce::Timer {
    fengyin::VlcVideoSurface surface;
    std::unique_ptr<juce::WebBrowserComponent> web;
    juce::File fixture;
    VideoFileChooser chooser { *this };
    bool chooserRequested=false,chooserHandled=false;
    int chooserCompletions=0;
    std::atomic<uint64_t> ui{now()},javascript{now()};
    bool failed=false,webOnly=false,ready=false,clickPending=false,expanded=false;
    int touchPhase=0,mousePhase=0;
    POINT touchPoint{},mousePoint{};
    const juce::String inputMode=juce::SystemStats::getEnvironmentVariable("FENGYIN_DIAGNOSTIC_INPUT_MODE","mixed");
    // Test pacing only: finish OS compatibility-pointer processing before the
    // next independent gesture. No delay is added to production controls.
    uint64_t inputSettledAt=0;
    bool touchInput()const{return inputMode=="touch"||(inputMode=="mixed"&&cycle%2);}
    bool controlPlaying=false;
    POINT clickPoint{};
    int phase=0,cycle=0,generation=0,requestedGeneration=1,playCommands=0,volumeCommands=0,webReadyGeneration=0;
    int previousPlay=0,previousVolume=0;
    uint64_t deadline=now()+30000,lastState=0,lastConfig=0;
    juce::String session;
    explicit ProductionUiScene(juce::File media,bool withoutNativeVideo=false):fixture(media),webOnly(withoutNativeVideo) {
        // Match the real DocumentWindow. A default non-opaque Component with
        // no title bar creates a layered HWND, which is a different host model.
        setOpaque(true);
        const auto options=juce::WebBrowserComponent::Options{}
            .withBackend(juce::WebBrowserComponent::Options::Backend::webview2)
            .withWinWebView2Options(juce::WebBrowserComponent::Options::WinWebView2{}
                .withUserDataFolder(juce::File::getSpecialLocation(juce::File::tempDirectory)
                    .getChildFile("fy-production-ui-"+juce::Uuid().toString())))
            .withNativeIntegrationEnabled()
            .withUserScript(R"JS(localStorage.setItem('fy-guide-seen','yes');localStorage.setItem('fy-voice-enabled','no');
                window.addEventListener('error',e=>window.__JUCE__.backend.emitEvent('testError',{message:e.message}));
                for(const event of ['pointerdown','pointerup','click','gotpointercapture','lostpointercapture'])document.addEventListener(event,e=>
                    window.__JUCE__.backend.emitEvent('testInput',{event,target:e.target.id,x:e.clientX,y:e.clientY,pointerId:e.pointerId,pointerType:e.pointerType}),true);
                window.addEventListener('load',()=>setInterval(()=>window.__JUCE__.backend.emitEvent('testHeartbeat',
                {expanded:document.body.classList.contains('video-expanded'),selected:
                    typeof nativeVideoEngine!=='undefined'&&nativeVideoEngine&&nativeAccompanimentReady?videoGeneration:0}),100));)JS")
            .withEventListener("testHeartbeat",[this](juce::var p){javascript.store(now());expanded=bool(p["expanded"]);webReadyGeneration=int(p["selected"]);})
            .withEventListener("webReady",[this](juce::var p){session=p["videoSession"].toString();ready=true;javascript.store(now());})
            .withEventListener("testRuntime",[](juce::var p){std::cout<<"production runtime "<<juce::JSON::toString(p,true)<<std::endl;})
            .withEventListener("testError",[this](juce::var p){failed=true;std::cerr<<"FAIL page script "<<juce::JSON::toString(p,true)<<std::endl;})
            .withEventListener("requestBackendState",[this](juce::var){lastConfig=0;})
            .withEventListener("testPoint",[this](juce::var p){
                std::cout<<"web point "<<juce::JSON::toString(p,true)<<std::endl;
                clickPoint={int(p["x"]),int(p["y"])};clickPending=true;
            })
            .withEventListener("testInput",[](juce::var p){std::cout<<"web input "<<juce::JSON::toString(p,true)<<std::endl;})
            .withEventListener("chooseVideo",[this](juce::var){
                chooserRequested=true;chooserHandled=false;
                chooser.open([this](juce::File file){
                    ++chooserCompletions;
                    if(!file.existsAsFile()){failed=true;return;}
                    ++generation;
                    if(!webOnly){
                        if(!surface.initialise()){failed=true;return;}
                        surface.player()->load(file.getFullPathName().toStdString(),generation);
                    }
                });
                // Duplicate browser events while a selector is open must not
                // create another dialog or load twice.
                chooser.open([this](juce::File){failed=true;});
            })
            .withEventListener("nativeVideoViewport",[this](juce::var p){
                if(webOnly)return;
                if(p["session"].toString()!=session||(bool(p["visible"])&&int(p["generation"])!=generation))return;
                std::cout<<"production viewport enter "<<juce::JSON::toString(p,true)<<std::endl;
                surface.updateViewport({int(p["x"]),int(p["y"]),int(p["width"]),int(p["height"])},bool(p["visible"]));
                std::cout<<"production viewport returned"<<std::endl;
            })
            .withEventListener("setVideoPlaybackState",[this](juce::var p){
                if(int(p["generation"])==generation){controlPlaying=bool(p["playing"]);if(!webOnly)surface.player()->setPlaying(controlPlaying);++playCommands;}
            })
            .withEventListener("setVideoVolume",[this](juce::var p){if(!webOnly)surface.player()->setVolume(float(p["value"]));++volumeCommands;})
            .withEventListener("seekVideo",[this](juce::var p){if(!webOnly)surface.player()->seek(double(p["position"]),int(p["seekRevision"]));});
        web=std::make_unique<juce::WebBrowserComponent>(options);
        if(!webOnly&&juce::SystemStats::getEnvironmentVariable("FENGYIN_DIAGNOSTIC_LEGACY_OVERLAP",{})!="1")
            surface.setWebViewCompanion(web.get());
        addAndMakeVisible(*web);addAndMakeVisible(surface);
        const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        const auto bounds=display?display->userArea.reduced(8):juce::Rectangle<int>(0,0,1200,760);
        setBounds(bounds);addToDesktop(0);
        if(!fengyin::configureVideoHostRendering(getPeer()))failed=true;
        if(!InitializeTouchInjection(10,TOUCH_FEEDBACK_NONE))failed=true;
        setVisible(true);toFront(true);web->setBounds(getLocalBounds());
        std::cout<<"production frontend="<<FENGYIN_DIAGNOSTIC_FRONTEND<<" viewport="<<getWidth()<<"x"<<getHeight()<<std::endl;
        if(webOnly)std::cout<<"CONTROL ONLY: no native HWND, decoder, first-picture or PCM acceptance"<<std::endl;
        web->goToURL(juce::URL(juce::File(FENGYIN_DIAGNOSTIC_FRONTEND)).toString(false));
        startTimer(20);
    }
    ~ProductionUiScene()override{stopTimer();surface.shutdown();web.reset();}
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff07101d));}
    void finish(){stopTimer();juce::MessageManager::getInstance()->stopDispatchLoop();}
    static juce::FileBrowserComponent* findFileBrowser(juce::Component* component) {
        if(auto* browser=dynamic_cast<juce::FileBrowserComponent*>(component))return browser;
        for(auto* child:component->getChildren())if(auto* browser=findFileBrowser(child))return browser;
        return nullptr;
    }
    void completeFileDialog() {
        if(!chooserRequested||chooserHandled)return;
        auto* dialog=dynamic_cast<juce::FileChooserDialogBox*>(juce::Component::getCurrentlyModalComponent());
        if(!dialog)return;
        if(auto* browser=findFileBrowser(dialog)){
            browser->setFileName(fixture.getFullPathName());
            // Exercise the actual chooser's asynchronous completion, including
            // destruction and its queued callback, rather than bypassing it.
            dialog->exitModalState(1);chooserHandled=true;
        }
    }
    void click(const juce::String& selector,double fraction=.5) {
        if(selector=="#change-video")requestedGeneration=generation+1;
        const auto script="(()=>{const e=document.querySelector("+juce::JSON::toString(selector)+");const r=e.getBoundingClientRect();"
            "const x=r.x+r.width*"+juce::String(fraction)+",y=r.y+r.height*.5;window.__JUCE__.backend.emitEvent('testPoint',"
            "{selector:"+juce::JSON::toString(selector)+",x,y,hit:document.elementFromPoint(x,y)?.id,dpr:devicePixelRatio,w:innerWidth,h:innerHeight});})()";
        web->evaluateJavascript(script);deadline=now()+10000;
    }
    void sendState() {
        auto state=std::make_unique<juce::DynamicObject>();
        state->setProperty("activated",true);state->setProperty("featuresUnlocked",true);
        state->setProperty("deviceConnected",true);state->setProperty("deviceMatched",true);
        state->setProperty("pluginLoaded",true);state->setProperty("pluginName","SWAM Cello 3 (UI fixture only)");
        state->setProperty("instrumentKey","cello");state->setProperty("instrumentChineseName",juce::String::fromUTF8("大提琴"));
        state->setProperty("breath",double((now()/100)%8)/10);state->setProperty("audioCpu",.4);
        state->setProperty("outputLeftPeak",.4);state->setProperty("outputRightPeak",.5);
        state->setProperty("instrumentPeak",.4);state->setProperty("musicTempoMode",1);
        if(now()-lastConfig>=1000) {
            lastConfig=now();web->emitEventIfBrowserIsVisible("backendState",juce::var(state.release()));
        } else {
            for(const char* key:{"activated","featuresUnlocked","deviceConnected","deviceMatched", "pluginLoaded",
                "pluginName","instrumentKey","instrumentChineseName","musicTempoMode"})state->removeProperty(key);
            web->emitEventIfBrowserIsVisible("backendMeterState",juce::var(state.release()));
        }
        if(generation==0)return;
        const auto s=surface.getState();auto video=std::make_unique<juce::DynamicObject>();
        video->setProperty("engine","vlc");video->setProperty("generation",generation);video->setProperty("viewportEpoch",0);
        video->setProperty("url",juce::URL(fixture).toString(false));video->setProperty("name",fixture.getFileName());
        video->setProperty("seekRevision",s.seekRevision);video->setProperty("ready",webOnly||(s.generation==generation&&s.ready));
        video->setProperty("playing",webOnly?controlPlaying:s.playing);video->setProperty("finished",s.finished);
        video->setProperty("duration",s.duration);video->setProperty("position",s.position);
        video->setProperty("error",juce::String::fromUTF8(s.error.c_str()));
        web->emitEventIfBrowserIsVisible("nativeVideoState",juce::var(video.release()));
    }
    bool firstPictureVisible() {
        const auto hwnd=static_cast<HWND>(surface.getHWND());RECT r{};
        if(!hwnd||!IsWindowVisible(hwnd)||!GetClientRect(hwnd,&r)||r.right<20||r.bottom<20)return false;
        POINT p{r.right/2,r.bottom/4};ClientToScreen(hwnd,&p);
        const auto dc=GetDC(nullptr);if(!dc)return false;
        const auto rgb=GetPixel(dc,p.x,p.y);ReleaseDC(nullptr,dc);
        return rgb!=CLR_INVALID&&GetRValue(rgb)>150&&GetGValue(rgb)<100&&GetBValue(rgb)<100;
    }
    bool injectTouch(POINTER_FLAGS flags) {
        POINTER_TOUCH_INFO contact{};
        contact.pointerInfo.pointerType=PT_TOUCH;contact.pointerInfo.pointerId=0;
        contact.pointerInfo.ptPixelLocation=touchPoint;contact.pointerInfo.pointerFlags=flags;
        contact.touchMask=TOUCH_MASK_CONTACTAREA|TOUCH_MASK_ORIENTATION|TOUCH_MASK_PRESSURE;
        contact.orientation=90;contact.pressure=512;
        contact.rcContact={touchPoint.x-2,touchPoint.y-2,touchPoint.x+2,touchPoint.y+2};
        for(int attempt=0;attempt<4;++attempt) {
            if(InjectTouchInput(1,&contact))return true;
            const auto error=GetLastError();
            if(error!=ERROR_NOT_READY){std::cerr<<"FAIL InjectTouchInput error="<<error<<" flags="<<flags<<std::endl;return false;}
            Sleep(1);
        }
        std::cerr<<"FAIL InjectTouchInput repeatedly not ready flags="<<flags<<std::endl;return false;
    }
    bool moveMouse(POINT p) {
        const auto left=GetSystemMetrics(SM_XVIRTUALSCREEN),top=GetSystemMetrics(SM_YVIRTUALSCREEN);
        const auto width=GetSystemMetrics(SM_CXVIRTUALSCREEN),height=GetSystemMetrics(SM_CYVIRTUALSCREEN);
        if(width<2||height<2)return false;
        INPUT input{};input.type=INPUT_MOUSE;
        input.mi.dx=LONG((int64_t(p.x-left)*65536)/width+32768/width);
        input.mi.dy=LONG((int64_t(p.y-top)*65536)/height+32768/height);
        input.mi.dwFlags=MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_VIRTUALDESK|MOUSEEVENTF_MOVE_NOCOALESCE;
        return SendInput(1,&input,sizeof(INPUT))==1;
    }
    void timerCallback()override {
        ui.store(now());
        if(surface.player()&&!surface.getState().error.empty()) {
            failed=true;std::cerr<<"FAIL production VLC engine: "<<surface.getState().error<<std::endl;
        }
        if(failed||now()>deadline||(ready&&now()-javascript.load()>5000)) {
            failed=true;std::cerr<<"FAIL production UI phase="<<phase<<" cycle="<<cycle
                <<" javascriptAge="<<now()-javascript.load()<<" deadlineExpired="<<(now()>deadline)<<std::endl;
            EnumChildWindows(static_cast<HWND>(getPeer()->getNativeHandle()),[](HWND child,LPARAM)->BOOL {
                wchar_t name[128]{};GetClassNameW(child,name,128);RECT r{};GetWindowRect(child,&r);
                DWORD pid=0;const auto thread=GetWindowThreadProcessId(child,&pid);
                GUITHREADINFO info{sizeof(info)};GetGUIThreadInfo(thread,&info);
                std::cout<<"child="<<child<<" class="<<juce::String(name)<<" thread="<<thread<<" pid="<<pid
                    <<" rect="<<r.left<<","<<r.top<<","<<r.right<<","<<r.bottom
                    <<" visible="<<IsWindowVisible(child)<<" enabled="<<IsWindowEnabled(child)
                    <<" capture="<<info.hwndCapture<<" focus="<<info.hwndFocus<<std::endl;
                return TRUE;
            },0);
            dumpProcess(L"production-ui-stall.dmp");finish();return;
        }
        if(touchPhase) {
            const auto flags=touchPhase==1?(POINTER_FLAG_UPDATE|POINTER_FLAG_INRANGE|POINTER_FLAG_INCONTACT):POINTER_FLAG_UP;
            if(!injectTouch(flags)){failed=true;return;}
            touchPhase=touchPhase==1?2:0;
            if(touchPhase==0)inputSettledAt=now()+250;
        }
        if(mousePhase) {
            if(mousePhase==3) {
                if(!moveMouse(mousePoint)){failed=true;return;}
                mousePhase=1;
            } else {
            INPUT input{};input.type=INPUT_MOUSE;
            input.mi.dwFlags=mousePhase==1?MOUSEEVENTF_LEFTDOWN:MOUSEEVENTF_LEFTUP;
            POINT actual{};
            if(!GetCursorPos(&actual)||std::abs(actual.x-mousePoint.x)>1||std::abs(actual.y-mousePoint.y)>1) {
                failed=true;std::cerr<<"FAIL mouse injection cursor moved before button event"<<std::endl;return;
            }
            if(SendInput(1,&input,sizeof(INPUT))!=1){failed=true;std::cerr<<"FAIL mouse button injection error="<<GetLastError()<<std::endl;return;}
            mousePhase=mousePhase==1?2:0;
            if(mousePhase==0)inputSettledAt=now()+250;
            }
        }
        if(clickPending&&touchPhase==0&&mousePhase==0&&now()>=inputSettledAt) {
            clickPending=false;
            const auto scale=getPeer()->getPlatformScaleFactor();
            POINT p{juce::roundToInt(clickPoint.x*scale),juce::roundToInt(clickPoint.y*scale)};
            ClientToScreen(static_cast<HWND>(getPeer()->getNativeHandle()),&p);
            wchar_t name[128]{};const auto hit=WindowFromPoint(p);GetClassNameW(hit,name,128);
            GUITHREADINFO info{sizeof(info)};GetGUIThreadInfo(GetWindowThreadProcessId(hit,nullptr),&info);
            RECT video{};GetWindowRect(static_cast<HWND>(surface.getHWND()),&video);
            std::cout<<"native input phase="<<phase<<" cycle="<<cycle<<" xy="<<p.x<<","<<p.y
                <<" scale="<<scale<<" hit="<<hit<<" class="<<juce::String(name)
                <<" video="<<video.left<<","<<video.top<<","<<video.right<<","<<video.bottom
                <<" capture="<<info.hwndCapture<<" focus="<<info.hwndFocus<<" foreground="<<GetForegroundWindow()<<std::endl;
            if(touchInput()) {
                touchPoint=p;touchPhase=1;
                if(!injectTouch(POINTER_FLAG_DOWN|POINTER_FLAG_INRANGE|POINTER_FLAG_INCONTACT)){failed=true;return;}
            } else {
                // Real mouse motion is required to leave Chromium's touch modality.
                // SetCursorPos alone is not a hardware mouse move. Separate motion,
                // down and up by message-loop turns, just as for touch injection.
                if(!moveMouse({p.x-4,p.y})){failed=true;std::cerr<<"FAIL mouse motion injection error="<<GetLastError()<<std::endl;return;}
                mousePoint=p;mousePhase=3;
            }
        }
        completeFileDialog();
        if(ready&&now()-lastState>=100){lastState=now();sendState();}
        if(phase==0&&ready) {
            web->evaluateJavascript("window.__JUCE__.backend.emitEvent('testRuntime',{userAgent:navigator.userAgent})");
            phase=1;click("#change-video");deadline=now()+30000;
        } else if(phase==1&&generation==requestedGeneration&&webReadyGeneration==generation&&(webOnly||(surface.getState().generation==generation&&surface.getState().ready&&firstPictureVisible()))) {
            std::cout<<(webOnly?"CONTROL fake selection generation=":"PASS real paused first picture generation=")<<generation<<std::endl;
            if(!webOnly&&generation==1&&surface.player()->testPcmNonzero.load()!=0){failed=true;return;}
            phase=2;click("#fullscreen");
        } else if(phase==2&&expanded==(cycle%2==0)) {
            previousPlay=playCommands;phase=3;click("#play-button");
        } else if(phase==3&&playCommands>previousPlay) {
            previousVolume=volumeCommands;phase=4;click("#video-volume",cycle%2?.25:.75);
        } else if(phase==4&&volumeCommands>previousVolume) {
            std::cout<<"PASS physical click cycle="<<cycle<<" production fullscreen/play/volume, input="<<(touchInput()?"touch":"mouse")<<" PCM="<<(webOnly?0:surface.player()->testPcmNonzero.load())<<std::endl;
            if(++cycle==20){if(chooserCompletions!=4){failed=true;std::cerr<<"FAIL picker completion count="<<chooserCompletions<<std::endl;}finish();return;}
            if(cycle%5==0){phase=1;click("#change-video");deadline=now()+30000;}
            else {phase=2;click("#fullscreen");}
        }
    }
};
