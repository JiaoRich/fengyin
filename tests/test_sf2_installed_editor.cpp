#include <juce_audio_utils/juce_audio_utils.h>
#include "../native/Sf2/Sf2State.h"
#include "../native/Source/NativeVideoFileDialog.h"
#include "NativeVideoDialogInput.h"
#include <iostream>
#include <stdexcept>

namespace {
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
void pump(int milliseconds=30) { juce::MessageManager::getInstance()->runDispatchLoopUntil(milliseconds); }
bool waitFor(const std::function<bool()>& predicate, int timeout=10000) {
    const auto end=juce::Time::getMillisecondCounterHiRes()+timeout;
    while(juce::Time::getMillisecondCounterHiRes()<end) { pump(); if(predicate())return true; }
    return false;
}
Microsoft::WRL::ComPtr<IUIAutomationElement> findText(HWND hwnd, const wchar_t* text) {
    Microsoft::WRL::ComPtr<IUIAutomation> automation;
    Microsoft::WRL::ComPtr<IUIAutomationElement> root,found;
    Microsoft::WRL::ComPtr<IUIAutomationCondition> condition;
    if(FAILED(CoCreateInstance(CLSID_CUIAutomation,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(&automation)))
        ||FAILED(automation->ElementFromHandle(hwnd,&root)))return {};
    VARIANT name;VariantInit(&name);name.vt=VT_BSTR;name.bstrVal=SysAllocString(text);
    const auto hr=automation->CreatePropertyCondition(UIA_NamePropertyId,name,&condition);VariantClear(&name);
    if(SUCCEEDED(hr))root->FindFirst(TreeScope_Subtree,condition.Get(),&found);
    return found;
}
void click(POINT p) {
    SetCursorPos(p.x,p.y); INPUT inputs[2]{};
    inputs[0].type=inputs[1].type=INPUT_MOUSE;
    inputs[0].mi.dwFlags=MOUSEEVENTF_LEFTDOWN;inputs[1].mi.dwFlags=MOUSEEVENTF_LEFTUP;
    require(SendInput(2,inputs,sizeof(INPUT))==2,"physical mouse input failed");
}
void clickChoose(juce::Component& window) {
    window.toFront(true);pump(200);
    const auto hwnd=static_cast<HWND>(window.getPeer()->getNativeHandle());
    Microsoft::WRL::ComPtr<IUIAutomationElement> button;
    require(waitFor([&]{button=findText(hwnd,L"选择 SF2 文件");return button!=nullptr;}),"real dynamic VST3 choose button missing");
    RECT bounds{};require(SUCCEEDED(button->get_CurrentBoundingRectangle(&bounds)),"button bounds unavailable");
    POINT point{(bounds.left+bounds.right)/2,(bounds.top+bounds.bottom)/2};
    require(GetAncestor(WindowFromPoint(point),GA_ROOT)==hwnd,"editor covered or coordinates wrong");
    click(point);
}
HWND findDialog() {
    struct Context {HWND found=nullptr;} context;
    EnumWindows([](HWND hwnd,LPARAM data)->BOOL {
        wchar_t title[256]{};GetWindowTextW(hwnd,title,256);
        if(juce::String(title)==juce::String::fromUTF8("选择 SF2 音源文件") && IsWindowVisible(hwnd))
            reinterpret_cast<Context*>(data)->found=hwnd;
        return TRUE;
    },reinterpret_cast<LPARAM>(&context));
    return context.found;
}
void selectFile(juce::Component& window,const juce::File& source,bool cancel) {
    clickChoose(window);
    HWND dialog=nullptr;require(waitFor([&]{dialog=findDialog();return dialog!=nullptr;}),"VST3's own picker never appeared");
    DWORD pid=0;GetWindowThreadProcessId(dialog,&pid);require(pid!=GetCurrentProcessId(),"picker is not isolated");
    auto process=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,FALSE,pid);require(process!=nullptr,"cannot inspect picker process");
    wchar_t path[32768]{};DWORD length=32768;const auto read=QueryFullProcessImageNameW(process,0,path,&length);CloseHandle(process);
    require(read && juce::File(juce::String(path))==juce::File::getSpecialLocation(juce::File::hostApplicationPath),"picker did not launch real host EXE");
    require(waitFor([&]{POINT point{};auto button=prepareNativeVideoDialog(dialog,source,cancel);
        if(!nativeDialogButtonPoint(dialog,button,point))return false;click(point);return true;}),"cannot physically click native Open/Cancel");
    require(waitFor([&]{return !IsWindow(dialog);}),"native dialog did not close");pump(400);
}
std::optional<fengyin::sf2::State> stateOf(juce::AudioPluginInstance& plugin) {
    juce::MemoryBlock bytes;plugin.getStateInformation(bytes);
    return fengyin::sf2::State::decode(bytes.getData(),int(bytes.getSize()));
}
double render(juce::AudioPluginInstance& plugin,juce::MidiBuffer midi={},int blocks=100) {
    juce::AudioBuffer<float> audio(2,128);double energy=0;
    for(int b=0;b<blocks;++b) {
        audio.clear();plugin.processBlock(audio,midi);midi.clear();
        for(int ch=0;ch<2;++ch)for(int i=0;i<128;++i){auto v=audio.getSample(ch,i);require(std::isfinite(v),"nonfinite PCM");if(b>blocks/2)energy+=v*v;}
    }return energy;
}
juce::MidiBuffer cc(int value,bool note=false) {
    juce::MidiBuffer midi;midi.addEvent(juce::MidiMessage::controllerEvent(1,11,value),0);
    if(note)midi.addEvent(juce::MidiMessage::noteOn(1,69,juce::uint8(90)),0);return midi;
}
}

int main() {
    const auto args=juce::StringArray::fromTokens(juce::String(GetCommandLineW()),true);
    if(args.size()==3 && args[1]=="--select-video")return fengyin::runNativeVideoFileDialog(juce::File(args[2].unquoted()));
    juce::ScopedJuceInitialiser_GUI gui;
    try {
        require(args.size()>=3,"plugin and fixture required");
        juce::AudioPluginFormatManager manager;manager.addFormat(std::make_unique<juce::VST3PluginFormat>());
        juce::OwnedArray<juce::PluginDescription> descriptions;
        manager.getFormat(0)->findAllTypesForFile(descriptions,args[1].unquoted());require(descriptions.size()==1,"staged VST3 discovery failed");
        juce::String error;auto plugin=manager.createPluginInstance(*descriptions[0],48000,128,error);
        require(plugin!=nullptr,error.toRawUTF8());plugin->prepareToPlay(48000,128);render(*plugin,{},2);
        juce::DocumentWindow window("Installed SF2 regression",juce::Colours::black,juce::DocumentWindow::allButtons);
        window.setUsingNativeTitleBar(true);window.setContentOwned(plugin->createEditorIfNeeded(),true);window.centreWithSize(650,450);window.setVisible(true);window.toFront(true);pump(600);
        if(args.contains("--expect-picker-failure")) {
            clickChoose(window);
            const auto hwnd=static_cast<HWND>(window.getPeer()->getNativeHandle());
            require(waitFor([&]{return findText(hwnd,L"无法打开音源选择窗口，请重试。原音源保持不变。")!=nullptr;}),"old code did not reproduce customer error");
            require(!findDialog(),"old code unexpectedly opened a picker");window.clearContentComponent();
            std::cout<<"PASS reproduced old dynamic VST3 editor failure (not static-linked picker test)\n";return 0;
        }
        const juce::File source(args[2].unquoted());
        selectFile(window,source,true);require(!stateOf(*plugin)->hasResource(),"cancel mutated empty state");
        selectFile(window,source,false);
        require(waitFor([&]{render(*plugin,{},2);auto s=stateOf(*plugin);return s && s->ready;},45000),"selected file did not commit");
        const auto selected=stateOf(*plugin);require(selected && selected->hasResource(),"state has no resource");
        const auto strong=render(*plugin,cc(110,true),240),weak=render(*plugin,cc(30),240),zero=render(*plugin,cc(0),240);
        require(strong>0.001 && weak>0 && weak<strong*.8 && zero<1e-9,"real selected SF2 CC11 PCM response wrong");
        juce::MemoryBlock saved;plugin->getStateInformation(saved);
        selectFile(window,source,true);require(stateOf(*plugin)->hash==selected->hash,"cancel lost loaded resource");
        if(auto stream=juce::File::getCurrentWorkingDirectory().getChildFile("sf2-installed-editor.png").createOutputStream())
            juce::PNGImageFormat().writeImageToStream(window.createComponentSnapshot(window.getLocalBounds()),*stream);
        window.clearContentComponent();plugin->releaseResources();plugin.reset();
        plugin=manager.createPluginInstance(*descriptions[0],48000,128,error);require(plugin!=nullptr,"reload instance failed");
        plugin->setStateInformation(saved.getData(),int(saved.getSize()));plugin->prepareToPlay(48000,128);
        require(waitFor([&]{render(*plugin,{},2);auto s=stateOf(*plugin);return s && s->ready;},45000),"saved SF2 failed to reload");
        auto restored=stateOf(*plugin);require(restored->hash==selected->hash && restored->bank==selected->bank && restored->program==selected->program && restored->gain==selected->gain && restored->bend==selected->bend,"restored parameters differ");
        require(render(*plugin,cc(100,true),240)>0.001,"reloaded resource silent");
        std::cout<<"PASS real dynamic VST3 editor / native mouse Open+Cancel / Unicode file / CC11 strong="<<strong<<" weak="<<weak<<" zero="<<zero<<" / complete state reload PCM\n";
        return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<std::endl;dumpNativeVideoDialog(GetCurrentProcessId());return 1;}
}
