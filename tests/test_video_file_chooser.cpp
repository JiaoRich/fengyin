#include "VideoFileChooser.h"
#include <iostream>
#include "NativeVideoDialogInput.h"

#if JUCE_WINDOWS
struct Test final : juce::Component, juce::Timer
{
    std::unique_ptr<VideoFileChooser> picker;
    juce::File folder, fixture;
    juce::File helperExecutable;
    int phase = 0, completed = 0, errors = 0, heartbeats = 0;
    bool failed = false, clicked = false;
    double seenAt = 0;
    const double deadline = juce::Time::getMillisecondCounterHiRes() + 60000;
    HANDLE destroyedProcess = nullptr;
    explicit Test(juce::File executable = {}) : helperExecutable(std::move(executable))
    {
        setOpaque(true); setBounds(20,20,800,600); addToDesktop(0); setVisible(true); toFront(true);
        folder = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile(juce::String::fromUTF8("风吟 中文😀选择器 ") + juce::Uuid().toString());
        folder.createDirectory(); fixture = folder.getChildFile(juce::String::fromUTF8("伴奏 测试.mp4")); fixture.replaceWithText("fixture");
        picker = std::make_unique<VideoFileChooser>(*this,helperExecutable); open(); startTimer(20);
    }
    ~Test() override { stopTimer(); picker.reset(); if(destroyedProcess)CloseHandle(destroyedProcess); folder.deleteRecursively(); }
    void fail(const char* message) {
        failed=true;std::cerr<<"FAIL "<<message<<" phase="<<phase<<" completed="<<completed<<" clicked="<<clicked<<" heartbeat="<<heartbeats<<std::endl;
        if(picker){std::cerr<<juce::JSON::toString(picker->nativeDiagnostics())<<std::endl;dumpNativeVideoDialog(picker->nativeProcessId());}
        juce::MessageManager::getInstance()->stopDispatchLoop();
    }
    void open()
    {
        clicked=false;seenAt=0;
        picker->open([this](juce::File file) {
            const auto expectedId=fixture.getFileIdentifier(),actualId=file.getFileIdentifier();
            std::cout<<"Native completion phase="<<phase<<" path="<<file.getFullPathName()<<" expected="<<fixture.getFullPathName()
                <<" ids="<<actualId<<"/"<<expectedId<<" pending="<<picker->isPending()<<std::endl;
            // Shell expands RUNNER~1/other 8.3 aliases. Compare the actual NTFS
            // file identity, not two spellings of the same absolute pathname.
            if(picker->isPending() || (phase==0 ? file.existsAsFile() : !file.existsAsFile()||actualId==0||actualId!=expectedId)) {fail("wrong native selection/cancellation");return;}
            ++completed;++phase;
            if(phase==2) {
                const auto area=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userBounds.toNearestInt();
                setBounds(area.getRight()-360,area.getBottom()-420,360,420);
            }
            if(phase==3)setBounds(juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userBounds.toNearestInt());
            if(phase<4)open();
            else if(phase==4) { // queue destruction: no callback, no helper leak
                auto temporary=std::make_unique<VideoFileChooser>(*this,helperExecutable);
                temporary->open([this](juce::File){fail("destroyed queued chooser callback");});temporary.reset();
                picker->open([this](juce::File){fail("destroyed active chooser callback");});phase=5;
            }
        },[this](juce::String){++errors;if(phase==7){phase=8;open();}else fail("native helper failed");});
        picker->open([this](juce::File){fail("duplicate picker callback");});
    }
    void timerCallback() override
    {
        ++heartbeats;
        if(juce::Time::getMillisecondCounterHiRes()>deadline){fail("native selector timeout");return;}
        if(phase==6) {
            if(WaitForSingleObject(destroyedProcess,0)==WAIT_TIMEOUT)return;
            CloseHandle(destroyedProcess);destroyedProcess=nullptr;
            picker=std::make_unique<VideoFileChooser>(*this,helperExecutable);
            phase=7;open();return;
        }
        if(phase==9) {
            if(completed!=5||errors!=1||heartbeats<20){fail("lifecycle counts or host heartbeat wrong");return;}
            std::cout<<"PASS native Shell cancel/open/Chinese+surrogate+space path/compact/fullscreen/duplicate/crash/reopen/destruction; UI heartbeats="<<heartbeats<<std::endl;
            juce::MessageManager::getInstance()->stopDispatchLoop();return;
        }
        const auto hwnd=picker->nativeWindow();
        if(!hwnd||clicked)return;
        DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);
        wchar_t name[64]{};GetClassNameW(hwnd,name,64);
        if(pid==GetCurrentProcessId()||pid!=picker->nativeProcessId()||juce::String(name)!="#32770"
            ||GetWindow(hwnd,GW_OWNER)!=nullptr||!IsWindowEnabled(static_cast<HWND>(getPeer()->getNativeHandle())))
        {fail("not an isolated native Windows file dialog");return;}
        if(phase==5) {
            destroyedProcess=OpenProcess(SYNCHRONIZE,FALSE,pid);
            if(!destroyedProcess){fail("cannot watch destroyed selector");return;}
            const auto before=juce::Time::getMillisecondCounterHiRes();picker.reset();
            if(juce::Time::getMillisecondCounterHiRes()-before>500){fail("destructor waited for Shell");return;}
            phase=6;return;
        }
        if(phase==7) {
            const auto process=OpenProcess(PROCESS_TERMINATE,FALSE,pid);
            if(!process){fail("cannot simulate helper crash");return;}
            TerminateProcess(process,77);CloseHandle(process);clicked=true;return;
        }
        if(!seenAt){seenAt=juce::Time::getMillisecondCounterHiRes();return;}
        if(juce::Time::getMillisecondCounterHiRes()-seenAt<250)return;
        const auto button=prepareNativeVideoDialog(hwnd,fixture,phase==0);
        POINT point{};
        if(!nativeDialogButtonPoint(hwnd,button,point))return;
        if(!SetCursorPos(point.x,point.y)){fail("cannot position mouse");return;}
        INPUT input[2]{};input[0].type=input[1].type=INPUT_MOUSE;
        input[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;input[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        if(SendInput(2,input,sizeof(INPUT))!=2){fail("cannot click actual Shell button");return;}
        clicked=true;
        std::cout<<"PASS physical native Shell selector phase="<<phase<<std::endl;
    }
};
#else

// Shared production selector. A modal object alone is not acceptance: require
// an independent, visible window and on Windows click the actual native peer.
struct Test final : juce::Component, juce::Timer
{
    std::unique_ptr<VideoFileChooser> picker;
    juce::File fixture;
    int completed=0,phase=0;
    bool failed=false,handled=false;
    double dialogSeen=0;
    const double deadline=juce::Time::getMillisecondCounterHiRes()+20000;
    Test() {
        setBounds(20,20,800,600);addToDesktop(0);setVisible(true);toFront(true);
        fixture=juce::File::getSpecialLocation(juce::File::tempDirectory)
            .getNonexistentChildFile(juce::String::fromUTF8("风吟-选择器-中文路径"), ".mp4");
        fixture.replaceWithText("chooser fixture only");
        picker=std::make_unique<VideoFileChooser>(*this);
        open();startTimer(20);
    }
    ~Test() override {stopTimer();picker.reset();fixture.deleteFile();}
    template<class T> static T* find(juce::Component* root) {
        if(auto* result=dynamic_cast<T*>(root))return result;
        for(auto* child:root->getChildren())if(auto* result=find<T>(child))return result;
        return nullptr;
    }
    static juce::TextButton* button(juce::Component* root,const juce::String& name) {
        if(auto* result=dynamic_cast<juce::TextButton*>(root))if(result->getButtonText()==name)return result;
        for(auto* child:root->getChildren())if(auto* result=button(child,name))return result;
        return nullptr;
    }
    void fail(const char* message) {
        failed=true;std::cerr<<"FAIL "<<message<<std::endl;
        juce::MessageManager::getInstance()->stopDispatchLoop();
    }
    void open() {
        handled=false;dialogSeen=0;
        picker->open([this](juce::File selected){
            ++completed;
            if(picker->isPending() || (phase==0?selected.existsAsFile():selected!=fixture))failed=true;
            if(++phase<4) {
                const auto area=juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userBounds.toNearestInt();
                if(phase==2)setBounds(area.getRight()-360,area.getBottom()-420,360,420);
                if(phase==3)setBounds(area);
                open();
            } else {
                auto disposable=std::make_unique<VideoFileChooser>(*this);
                disposable->open([this](juce::File){failed=true;});
                disposable.reset(); // queued opener must be invalidated
                phase=5;
            }
        });
        picker->open([this](juce::File){failed=true;});
    }
    void timerCallback() override {
        if(phase==5) {
            if(completed!=4 || juce::Component::getCurrentlyModalComponent()!=nullptr)failed=true;
            // Also destroy a selector while its desktop dialog is active.
            picker->open([this](juce::File){failed=true;});phase=6;return;
        }
        if(phase==7) {
            if(juce::Component::getCurrentlyModalComponent()!=nullptr)failed=true;
            juce::MessageManager::getInstance()->stopDispatchLoop();return;
        }
        if(juce::Time::getMillisecondCounterHiRes()>deadline){fail("selector lifecycle timed out");return;}
        if(handled && phase!=6)return;
        auto* dialog=dynamic_cast<juce::FileChooserDialogBox*>(juce::Component::getCurrentlyModalComponent());
        if(!dialog)return;
        if(!dialog->isOnDesktop() || dialog->getParentComponent()!=nullptr
           || dialog->getPeer()==nullptr || dialog->getPeer()==getPeer()) {
            fail("video chooser has no independent desktop peer; native web child can cover it");return;
        }
        if(phase==6){picker.reset();phase=7;return;}
        auto* b=find<juce::FileBrowserComponent>(dialog);
        if(!b)return;
        if(dialogSeen==0){dialogSeen=juce::Time::getMillisecondCounterHiRes();if(phase!=0)b->setFileName(fixture.getFullPathName());return;}
        if(juce::Time::getMillisecondCounterHiRes()-dialogSeen<150)return;
        const auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(dialog->getScreenBounds());
        if(!dialog->isShowing() || !display || !display->userBounds.toNearestInt().contains(dialog->getScreenBounds())){
            fail("selector is hidden or outside the usable screen");return;
        }
        auto* target=button(dialog,phase==0?"Cancel":b->getActionVerb());
        if(!target || !target->isEnabled())return;
       #if JUCE_WINDOWS
        const auto hwnd=static_cast<HWND>(dialog->getPeer()->getNativeHandle());
        if(!IsWindowVisible(hwnd) || !IsWindowEnabled(hwnd)
           || (GetWindowLongPtrW(hwnd,GWL_STYLE)&WS_CHILD)!=0
           || GetWindow(hwnd,GW_OWNER)!=static_cast<HWND>(getPeer()->getNativeHandle())) {
            fail("selector HWND visibility, top-level style or host ownership is wrong");return;
        }
        const auto local=dialog->getLocalPoint(target,target->getLocalBounds().getCentre());
        const auto scale=dialog->getPeer()->getPlatformScaleFactor();
        POINT point{juce::roundToInt(local.x*scale),juce::roundToInt(local.y*scale)};ClientToScreen(hwnd,&point);
        if(GetAncestor(WindowFromPoint(point),GA_ROOT)!=hwnd || GetForegroundWindow()!=hwnd) {
            fail("selector button is covered or cannot receive physical input");return;
        }
        if(!SetCursorPos(point.x,point.y)){fail("cannot position test mouse");return;}
        INPUT input[2]{};input[0].type=input[1].type=INPUT_MOUSE;
        input[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;input[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
        if(SendInput(2,input,sizeof(INPUT))!=2){fail("cannot click actual selector button");return;}
       #else
        target->triggerClick(); // Windows CI additionally requires physical HWND input.
       #endif
        handled=true;
        std::cout<<"PASS visible desktop selector "<<phase<<" normal/compact/expanded button path\n";
    }
};
#endif
int main() {
   #if JUCE_WINDOWS
    const auto args=juce::StringArray::fromTokens(juce::String(GetCommandLineW()),true);
    if(args.size()==3&&args[1]=="--select-video")return fengyin::runNativeVideoFileDialog(juce::File(args[2].unquoted()));
    const auto helper=args.size()==3&&args[1]=="--production-picker"?juce::File(args[2].unquoted()):juce::File();
   #endif
    juce::ScopedJuceInitialiser_GUI gui;
   #if JUCE_WINDOWS
    Test test(helper);
   #else
    Test test;
   #endif
    juce::MessageManager::getInstance()->runDispatchLoop();
    std::cout<<(test.failed?"FAIL":"PASS")<<" asynchronous production video selector visibility and lifecycle\n";
    return test.failed?1:0;
}
