#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "VlcPlaybackEngine.h"

namespace fengyin
{
#if JUCE_WINDOWS
class VlcVideoSurface final : public juce::HWNDComponent
#else
class VlcVideoSurface final : public juce::Component
#endif
{
public:
    ~VlcVideoSurface() override;
    bool initialise();
    void shutdown();
    void updateViewport(juce::Rectangle<int>, bool visible);
    VlcPlaybackEngine* player() noexcept { return engine.get(); }
    VlcPlaybackEngine::State getState() const { return engine ? engine->getState() : VlcPlaybackEngine::State{}; }
    static constexpr bool supported() noexcept {
#if JUCE_WINDOWS
        return true;
#else
        return false;
#endif
    }
private:
    std::unique_ptr<VlcPlaybackEngine> engine;
};
}
