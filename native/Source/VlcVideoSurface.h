#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "VlcPlaybackEngine.h"

namespace fengyin
{
class VlcVideoSurface final : public juce::Component
{
public:
    VlcVideoSurface() { setInterceptsMouseClicks(false,false);setWantsKeyboardFocus(false); }
    ~VlcVideoSurface() override;
    bool initialise();
    void shutdown();
    void updateViewport(juce::Rectangle<int>, bool visible);
    VlcPlaybackEngine* player() noexcept { return engine.get(); }
    void* getHWND() const noexcept { return nativeWindow; }
    VlcPlaybackEngine::State getState() const { return engine ? engine->getState() : VlcPlaybackEngine::State{}; }
    static constexpr bool supported() noexcept {
#if JUCE_WINDOWS
        return true;
#else
        return false;
#endif
    }
private:
    void* nativeWindow=nullptr;
    std::unique_ptr<VlcPlaybackEngine> engine;
};
}
