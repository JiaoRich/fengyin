#include "VlcVideoSurface.h"
#include "VlcNativeWindow.h"
#if JUCE_WINDOWS
#include <windows.h>
#endif

namespace fengyin
{
VlcVideoSurface::~VlcVideoSurface() { shutdown(); }
bool VlcVideoSurface::initialise()
{
#if JUCE_WINDOWS
    if(engine)return true;
    // Keep the JUCE placeholder visible and non-interactive. Its visibility
    // must not drive HWNDComponent's peer/visibility callbacks during a
    // WebView2 event. The video HWND remains a stable child for its lifetime;
    // only explicit Win32 viewport operations show/hide it, never SetParent.
    const auto parent=getPeer()?static_cast<HWND>(getPeer()->getNativeHandle()):nullptr;
    setVisible(true);
    if(!isShowing())return false;
    auto hwnd=createVlcChildWindow(parent);
    if(!hwnd)return false;
    nativeWindow=hwnd;
    auto root=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("components/VLC");
    try {
        engine=std::make_unique<VlcPlaybackEngine>(std::filesystem::path(root.getFullPathName().toWideCharPointer()),hwnd);
    } catch(const std::exception& error) {
        juce::Logger::writeToLog("VLC worker creation failed: "+juce::String(error.what()));
        nativeWindow=nullptr;DestroyWindow(hwnd);return false;
    }
    return true;
#else
    return false;
#endif
}
void VlcVideoSurface::shutdown()
{
    if(!engine)return;
    engine->requestShutdown();
#if JUCE_WINDOWS
    // Direct3D renderer teardown can SendMessage to its UI-owned child HWND.
    // Drain sent messages, not user input/timers, so closing cannot deadlock or
    // re-enter MainComponent after destruction has begun. Never kill a thread.
    while(!engine->hasStopped()) {
        MSG message{};
        PeekMessageW(&message,nullptr,WM_NULL,WM_NULL,PM_NOREMOVE);
        Sleep(1);
    }
#endif
    engine.reset();
#if JUCE_WINDOWS
    const auto hwnd=static_cast<HWND>(nativeWindow);nativeWindow=nullptr;
    if(hwnd)DestroyWindow(hwnd);
#endif
}
void VlcVideoSurface::updateViewport(juce::Rectangle<int> bounds,bool visible)
{
    const bool show=visible && !bounds.isEmpty() && engine!=nullptr;
    // Never resize a paused/hidden renderer to a zero-sized swapchain. Page
    // hiding only hides the native HWND; the next valid viewport sets geometry.
    if(!bounds.isEmpty() && getBounds()!=bounds)setBounds(bounds);
#if JUCE_WINDOWS
    const auto parent=getPeer()?static_cast<HWND>(getPeer()->getNativeHandle()):nullptr;
    const auto hwnd=static_cast<HWND>(nativeWindow);
    if(hwnd && !bounds.isEmpty() && getPeer()) {
        const auto area=(getPeer()->getAreaCoveredBy(*this).toFloat()
            *getPeer()->getPlatformScaleFactor()).getSmallestIntegerContainer();
        const auto oldContext=SetThreadDpiAwarenessContext(GetWindowDpiAwarenessContext(hwnd));
        SetWindowPos(hwnd,nullptr,area.getX(),area.getY(),area.getWidth(),area.getHeight(),
            SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOOWNERZORDER);
        if(oldContext)SetThreadDpiAwarenessContext(oldContext);
    }
    setVlcChildVisible(hwnd,parent,show && isShowing());
#endif
}
}
