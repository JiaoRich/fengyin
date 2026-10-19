#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace fengyin {
// The host only paints a static backing colour. WebView2 and VLC are native
// children with their own GPU renderers; an additional Direct2D swapchain for
// the obscured dashboard is unnecessary. This does NOT turn off video or web
// hardware acceleration, and does not change third-party plug-in editors.
inline bool configureVideoHostRendering(juce::ComponentPeer* peer)
{
#if JUCE_WINDOWS
    if (peer == nullptr) return false;
    const auto engines = peer->getAvailableRenderingEngines();
    const auto software = engines.indexOf("Software Renderer");
    if (software < 0) return false;
    peer->setCurrentRenderingEngine(software);
    return peer->getCurrentRenderingEngine() == software;
#else
    juce::ignoreUnused(peer);
    return false;
#endif
}
}
