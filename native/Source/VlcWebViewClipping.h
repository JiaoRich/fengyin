#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#if JUCE_WINDOWS
#include <windows.h>
namespace fengyin {
// Resolve JUCE's registered native browser child, not a guessed Chromium class
// name or a cross-process renderer window. Only modify our own hosted HWND.
inline HWND nativeWebViewHost(juce::Component& component,HWND parent) {
    if(auto hwnd=static_cast<HWND>(juce::AccessibilityHandler::getNativeChildForComponent(component))) {
        DWORD pid=0;GetWindowThreadProcessId(hwnd,&pid);
        if(IsWindow(hwnd)&&pid==GetCurrentProcessId()&&GetParent(hwnd)==parent)return hwnd;
    }
    for(int i=0;i<component.getNumChildComponents();++i)
        if(auto hwnd=nativeWebViewHost(*component.getChildComponent(i),parent))return hwnd;
    return nullptr;
}
inline bool clipWebViewForVideo(juce::Component& browser,HWND video,bool visible) {
    if(!browser.getPeer())return false;
    const auto parent=static_cast<HWND>(browser.getPeer()->getNativeHandle());
    const auto host=nativeWebViewHost(browser,parent);
    if(!host)return false;
    if(!visible)return SetWindowRgn(host,nullptr,TRUE)!=0;
    RECT area{},picture{};
    if(!IsWindow(video)||!GetClientRect(host,&area)||!GetWindowRect(video,&picture))return false;
    MapWindowPoints(nullptr,host,reinterpret_cast<POINT*>(&picture),2);
    const auto region=CreateRectRgn(area.left,area.top,area.right,area.bottom);
    const auto hole=CreateRectRgn(picture.left,picture.top,picture.right,picture.bottom);
    if(!region||!hole){if(region)DeleteObject(region);if(hole)DeleteObject(hole);return false;}
    const auto combined=CombineRgn(region,region,hole,RGN_DIFF);DeleteObject(hole);
    if(combined==ERROR||SetWindowRgn(host,region,TRUE)==0){DeleteObject(region);return false;}
    // Windows owns region after successful SetWindowRgn.
    return true;
}
}
#endif
