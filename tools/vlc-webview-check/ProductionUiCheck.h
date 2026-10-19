#pragma once
// Diagnostic only. Real release HTML/CSS/JS, file:// origin, physical input,
// production VLC/host renderer. Fake meter data is NOT an instrument DSP test.
struct ProductionUiScene final:juce::Component,juce::Timer {
    fengyin::VlcVideoSurface surface;
    std::unique_ptr<juce::WebBrowserComponent> web;
    juce::File fixture;
    std::atomic<uint64_t> ui{now()},javascript{now()};
    bool failed=false,webOnly=false,ready=false,clickPending=false,expanded=false,touchDown=false;
    POINT clickPoint{};
    int phase=0,cycle=0,generation=0,playCommands=0,volumeCommands=0;
    int previousPlay=0,previousVolume=0;
    uint64_t deadline=now()+30000,lastState=0,lastConfig=0;
    juce::String session;
    explicit ProductionUiScene(juce::File media):fixture(media) {
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
                for(const event of ['pointerdown','pointerup','click'])document.addEventListener(event,e=>
                    window.__JUCE__.backend.emitEvent('testInput',{event,target:e.target.id,x:e.clientX,y:e.clientY}),true);
                window.addEventListener('load',()=>setInterval(()=>window.__JUCE__.backend.emitEvent('testHeartbeat',
                {expanded:document.body.classList.contains('video-expanded')}),100));)JS")
            .withEventListener("testHeartbeat",[this](juce::var p){javascript.store(now());expanded=bool(p["expanded"]);})
            .withEventListener("webReady",[this](juce::var p){session=p["videoSession"].toString();ready=true;javascript.store(now());})
            .withEventListener("requestBackendState",[this](juce::var){lastConfig=0;})
            .withEventListener("testPoint",[this](juce::var p){
                std::cout<<"web point "<<juce::JSON::toString(p,true)<<std::endl;
                clickPoint={int(p["x"]),int(p["y"])};clickPending=true;
            })
            .withEventListener("testInput",[](juce::var p){std::cout<<"web input "<<juce::JSON::toString(p,true)<<std::endl;})
            .withEventListener("chooseVideo",[this](juce::var){
                surface.player()->load(fixture.getFullPathName().toStdString(),++generation);
            })
            .withEventListener("nativeVideoViewport",[this](juce::var p){
                if(p["session"].toString()!=session||(bool(p["visible"])&&int(p["generation"])!=generation))return;
                surface.updateViewport({int(p["x"]),int(p["y"]),int(p["width"]),int(p["height"])},bool(p["visible"]));
            })
            .withEventListener("setVideoPlaybackState",[this](juce::var p){
                if(int(p["generation"])==generation){surface.player()->setPlaying(bool(p["playing"]));++playCommands;}
            })
            .withEventListener("setVideoVolume",[this](juce::var p){surface.player()->setVolume(float(p["value"]));++volumeCommands;})
            .withEventListener("seekVideo",[this](juce::var p){surface.player()->seek(double(p["position"]),int(p["seekRevision"]));});
        web=std::make_unique<juce::WebBrowserComponent>(options);
        addAndMakeVisible(*web);addAndMakeVisible(surface);
        const auto* display=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
        const auto bounds=display?display->userArea.reduced(8):juce::Rectangle<int>(0,0,1200,760);
        setBounds(bounds);addToDesktop(0);
        if(!fengyin::configureVideoHostRendering(getPeer()))failed=true;
        if(!InitializeTouchInjection(1,TOUCH_FEEDBACK_NONE))failed=true;
        setVisible(true);toFront(true);web->setBounds(getLocalBounds());
        std::cout<<"production frontend="<<FENGYIN_DIAGNOSTIC_FRONTEND<<" viewport="<<getWidth()<<"x"<<getHeight()<<std::endl;
        web->goToURL(juce::URL(juce::File(FENGYIN_DIAGNOSTIC_FRONTEND)).toString(false));
        startTimer(20);
    }
    ~ProductionUiScene()override{stopTimer();surface.shutdown();web.reset();}
    void paint(juce::Graphics& g)override{g.fillAll(juce::Colour(0xff07101d));}
    void finish(){stopTimer();juce::MessageManager::getInstance()->stopDispatchLoop();}
    void click(const juce::String& selector,double fraction=.5) {
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
        video->setProperty("seekRevision",s.seekRevision);video->setProperty("ready",s.generation==generation&&s.ready);
        video->setProperty("playing",s.playing);video->setProperty("finished",s.finished);
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
        contact.pointerInfo.pointerType=PT_TOUCH;contact.pointerInfo.pointerId=1;
        contact.pointerInfo.ptPixelLocation=clickPoint;contact.pointerInfo.pointerFlags=flags;
        contact.touchMask=TOUCH_MASK_CONTACTAREA|TOUCH_MASK_ORIENTATION|TOUCH_MASK_PRESSURE;
        contact.orientation=90;contact.pressure=512;
        contact.rcContact={clickPoint.x-2,clickPoint.y-2,clickPoint.x+2,clickPoint.y+2};
        return InjectTouchInput(1,&contact)!=FALSE;
    }
    void timerCallback()override {
        ui.store(now());
        if(surface.player()&&!surface.getState().error.empty()) {
            failed=true;std::cerr<<"FAIL production VLC engine: "<<surface.getState().error<<std::endl;
        }
        if(failed||now()>deadline||(ready&&now()-javascript.load()>5000)) {
            failed=true;std::cerr<<"FAIL production UI phase="<<phase<<" cycle="<<cycle<<std::endl;
            dumpProcess(L"production-ui-stall.dmp");finish();return;
        }
        if(touchDown) {
            touchDown=false;
            if(!injectTouch(POINTER_FLAG_UP)){failed=true;return;}
        }
        if(clickPending) {
            clickPending=false;
            const auto scale=getPeer()->getPlatformScaleFactor();
            POINT p{juce::roundToInt(clickPoint.x*scale),juce::roundToInt(clickPoint.y*scale)};
            ClientToScreen(static_cast<HWND>(getPeer()->getNativeHandle()),&p);
            wchar_t name[128]{};const auto hit=WindowFromPoint(p);GetClassNameW(hit,name,128);
            RECT video{};GetWindowRect(static_cast<HWND>(surface.getHWND()),&video);
            std::cout<<"native input phase="<<phase<<" cycle="<<cycle<<" xy="<<p.x<<","<<p.y
                <<" scale="<<scale<<" hit="<<hit<<" class="<<juce::String(name)
                <<" video="<<video.left<<","<<video.top<<","<<video.right<<","<<video.bottom
                <<" capture="<<GetCapture()<<" foreground="<<GetForegroundWindow()<<std::endl;
            if(cycle%2) {
                clickPoint=p;touchDown=true;
                if(!injectTouch(POINTER_FLAG_DOWN|POINTER_FLAG_INRANGE|POINTER_FLAG_INCONTACT)){failed=true;return;}
            } else {
                SetCursorPos(p.x,p.y);INPUT input[2]{};input[0].type=input[1].type=INPUT_MOUSE;
                input[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;input[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
                if(SendInput(2,input,sizeof(INPUT))!=2){failed=true;return;}
            }
        }
        if(ready&&now()-lastState>=100){lastState=now();sendState();}
        if(phase==0&&ready) {
            if(!surface.initialise()){failed=true;return;}
            phase=1;click("#change-video");deadline=now()+30000;
        } else if(phase==1&&surface.getState().generation==generation&&surface.getState().ready&&firstPictureVisible()) {
            std::cout<<"PASS real paused first picture generation="<<generation<<std::endl;
            if(generation==1&&surface.player()->testPcmNonzero.load()!=0){failed=true;return;}
            phase=2;click("#fullscreen");
        } else if(phase==2&&expanded==(cycle%2==0)) {
            previousPlay=playCommands;phase=3;click("#play-button");
        } else if(phase==3&&playCommands>previousPlay) {
            previousVolume=volumeCommands;phase=4;click("#video-volume",cycle%2?.25:.75);
        } else if(phase==4&&volumeCommands>previousVolume) {
            std::cout<<"PASS physical click cycle="<<cycle<<" production fullscreen/play/volume, input="<<(cycle%2?"touch":"mouse")<<" PCM="<<surface.player()->testPcmNonzero.load()<<std::endl;
            if(++cycle==20){finish();return;}
            if(cycle%5==0){phase=1;click("#change-video");deadline=now()+30000;}
            else {phase=2;click("#fullscreen");}
        }
    }
};
