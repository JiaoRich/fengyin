#pragma once
#if defined(_WIN32)
#include <windows.h>

namespace fengyin {
inline HWND createVlcChildWindow(HWND parent) noexcept {
    if(!IsWindow(parent))return nullptr;
    return CreateWindowExW(WS_EX_NOACTIVATE,L"STATIC",L"FengYin VLC video",
        WS_CHILD|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|SS_BLACKRECT,
        0,0,16,16,parent,nullptr,GetModuleHandleW(nullptr),nullptr);
}
inline bool setVlcChildVisible(HWND window,HWND parent,bool show) noexcept {
    if(!IsWindow(window))return false;
    const auto style=GetWindowLongPtrW(window,GWL_STYLE);
    const auto exStyle=GetWindowLongPtrW(window,GWL_EXSTYLE);
    const bool valid=IsWindow(parent) && GetParent(window)==parent
        && (style&WS_CHILD)!=0 && (style&WS_POPUP)==0 && (exStyle&WS_EX_TOPMOST)==0;
    if(!show || !valid || IsIconic(parent) || !IsWindowVisible(parent)) {
        ShowWindow(window,SW_HIDE);
        return !show || valid;
    }
    // HWND_TOP orders siblings only: a video child cannot become a desktop
    // floating window. Hide is explicit even if JUCE's cached flag is false.
    return SetWindowPos(window,HWND_TOP,0,0,0,0,
        SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOOWNERZORDER|SWP_SHOWWINDOW)!=0;
}
}
#endif
