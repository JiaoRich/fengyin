#include "VideoFileChooser.h"
#include <iostream>

// Shared production selector: cancellation, duplicate events, confirmation,
// reopening, and destruction with a queued open. Real async JUCE modal UI.
struct Test final : juce::Component, juce::Timer
{
    std::unique_ptr<VideoFileChooser> picker;
    juce::File fixture;
    int completed=0,phase=0;
    bool failed=false,handled=false;
    const double deadline=juce::Time::getMillisecondCounterHiRes()+15000;
    Test() {
        setBounds(20,20,800,600);addToDesktop(0);setVisible(true);
        fixture=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fy-picker", ".mp4");
        fixture.replaceWithText("chooser fixture only");
        picker=std::make_unique<VideoFileChooser>(*this);
        open();startTimer(20);
    }
    ~Test() override {stopTimer();picker.reset();fixture.deleteFile();}
    static juce::FileBrowserComponent* browser(juce::Component* root) {
        if(auto* result=dynamic_cast<juce::FileBrowserComponent*>(root))return result;
        for(auto* child:root->getChildren())if(auto* result=browser(child))return result;
        return nullptr;
    }
    void open() {
        handled=false;
        picker->open([this](juce::File selected){
            ++completed;
            if(picker->isPending() || (phase==0?selected.existsAsFile():selected!=fixture))failed=true;
            if(++phase<4)open();
            else {
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
            juce::MessageManager::getInstance()->stopDispatchLoop();return;
        }
        if(juce::Time::getMillisecondCounterHiRes()>deadline){failed=true;juce::MessageManager::getInstance()->stopDispatchLoop();return;}
        if(handled)return;
        auto* dialog=dynamic_cast<juce::FileChooserDialogBox*>(juce::Component::getCurrentlyModalComponent());
        if(dialog)if(auto* b=browser(dialog)) {
            if(phase!=0)b->setFileName(fixture.getFullPathName());
            handled=true;dialog->exitModalState(phase==0?0:1);
        }
    }
};
int main() {
    juce::ScopedJuceInitialiser_GUI gui;
    Test test;juce::MessageManager::getInstance()->runDispatchLoop();
    std::cout<<(test.failed?"FAIL":"PASS")<<" asynchronous production video selector lifecycle\n";
    return test.failed?1:0;
}
