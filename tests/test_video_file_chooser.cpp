#include "VideoFileChooser.h"
#include <iostream>

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
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    Test test;juce::MessageManager::getInstance()->runDispatchLoop();
    std::cout<<(test.failed?"FAIL":"PASS")<<" asynchronous production video selector visibility and lifecycle\n";
    return test.failed?1:0;
}
