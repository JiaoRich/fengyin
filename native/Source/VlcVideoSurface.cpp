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
    // Attach before VLC creates its render-thread children. It must never
    // see a free-standing popup while the web page is still hidden.
    const auto parent=getPeer()?static_cast<HWND>(getPeer()->getNativeHandle()):nullptr;
    auto hwnd=createVlcChildWindow(parent);
    if(!hwnd)return false;
    setHWND(hwnd);
    auto root=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory().getChildFile("components/VLC");
    engine=std::make_unique<VlcPlaybackEngine>(std::filesystem::path(root.getFullPathName().toWideCharPointer()),hwnd);
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
    setHWND(nullptr);
#endif
}
void VlcVideoSurface::updateViewport(juce::Rectangle<int> bounds,bool visible)
{
    const bool show=visible && !bounds.isEmpty() && engine!=nullptr;
    if(getBounds()!=bounds)setBounds(bounds);
    if(isVisible()!=show)setVisible(show);
#if JUCE_WINDOWS
    const auto parent=getPeer()?static_cast<HWND>(getPeer()->getNativeHandle()):nullptr;
    setVlcChildVisible(static_cast<HWND>(getHWND()),parent,show && isShowing());
#endif
}
}
