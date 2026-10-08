#pragma once
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <juce_gui_basics/juce_gui_basics.h>
#if JUCE_WINDOWS
 #include <windows.h>
#endif
#include <cmath>
#include <vector>

// Separate transparent peer: never changes the opaque WebView2 host peer.
// Parameters follow reference-edge-glow.html. No mouse input or taskbar entry.
class CompactWindowGlow final : public juce::Component
{
public:
    CompactWindowGlow()
    {
        setOpaque(false);
        setInterceptsMouseClicks(false, false);
        setWantsKeyboardFocus(false);
    }
    void update(juce::Component& owner, bool enabled, float breath, float radius = 28.0f)
    {
        if (!enabled) { setVisible(false); last = 0; return; }
        if (getPeer() == nullptr)
        {
            addToDesktop(juce::ComponentPeer::windowIsTemporary
                         | juce::ComponentPeer::windowIgnoresMouseClicks);
           #if JUCE_WINDOWS
            // Use the per-pixel layered-window software path, independent of
            // WebView/Direct2D swap-chain composition on the opaque host.
            if (auto* peer = getPeer()) peer->setCurrentRenderingEngine(0);
           #endif
        }
        const bool shapeChanged = cornerRadius != radius;
        cornerRadius = radius;
        const auto bounds = owner.getScreenBounds().expanded(padding);
        if (getBounds() != bounds || shapeChanged || frame.isNull())
        {
            const bool sizeChanged = getWidth() != bounds.getWidth() || getHeight() != bounds.getHeight();
            setBounds(bounds);
            if (sizeChanged || shapeChanged || frame.isNull()) rebuild();
        }
       #if JUCE_WINDOWS
        if (owner.getPeer() != nullptr && getPeer() != nullptr)
        {
            const auto host = static_cast<HWND>(owner.getPeer()->getNativeHandle());
            const auto halo = static_cast<HWND>(getPeer()->getNativeHandle());
            if (GetWindowLongPtr(halo, GWLP_HWNDPARENT) != reinterpret_cast<LONG_PTR>(host))
                SetWindowLongPtr(halo, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(host));
            // Owned windows stay above the owner, but the entire interior is
            // transparent. Never take activation or push the app above others.
            SetWindowPos(halo, nullptr, 0, 0, 0, 0,
                         SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
            if (GetAncestor(GetForegroundWindow(), GA_ROOT) == host)
                SetWindowPos(halo, HWND_TOP, 0, 0, 0, 0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
        }
       #endif
        setVisible(true);
        const auto now = juce::Time::getMillisecondCounterHiRes() * .001;
        const auto dt = last == 0 ? 0.0 : juce::jlimit(0.0, .1, now - last);
        last = now;
        const auto target = juce::jlimit(0.0, 1.0, static_cast<double>(breath));
        energy += (target - energy) * (1.0 - std::exp(-dt / (target > energy ? .12 : .32)));
        phase = std::fmod(phase + dt / 4.0, 1.0);
        render();
        repaint();
    }
    void paint(juce::Graphics& g) override { g.drawImageAt(frame, 0, 0); }

private:
    friend struct CompactWindowGlowTestAccess;
    static constexpr int padding = 48;
    struct Pixel { int x, y; float distance, angle; };
    std::vector<Pixel> pixels;
    juce::Image frame;
    double energy = 0, phase = 0, last = 0;
    float cornerRadius = 28.0f;
    void rebuild()
    {
        frame = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
        pixels.clear();
        const auto cx = getWidth() * .5f, cy = getHeight() * .5f;
        const auto hx = cx - padding, hy = cy - padding;
        const float radius = cornerRadius;
        for (int y = 0; y < getHeight(); ++y)
            for (int x = 0; x < getWidth(); ++x)
            {
                const auto qx = std::abs(x + .5f - cx) - (hx - radius);
                const auto qy = std::abs(y + .5f - cy) - (hy - radius);
                const auto d = std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f))
                    + std::min(std::max(qx, qy), 0.0f) - radius;
                if (d < -.5f || d > padding) continue; // subpixel edge coverage
                auto angle = std::atan2(y + .5f - cy, x + .5f - cx)
                    / juce::MathConstants<float>::twoPi;
                if (angle < 0) angle += 1;
                pixels.push_back({x, y, d, angle});
            }
    }
    void render()
    {
        if (frame.isNull()) return;
        const juce::Colour colours[] { juce::Colour(0xff00eaff), juce::Colour(0xff3654ff),
            juce::Colour(0xffe33dff), juce::Colour(0xffff613a), juce::Colour(0xffffe438),
            juce::Colour(0xff00f2a6), juce::Colour(0xff00eaff) };
        const double e = energy * .35;
        const double layers[][3] {{30,22,.85},{14,9,.95},{4,3,1},{0,1.4,1}};
        float alpha[193] {};
        for (int i = 0; i < 193; ++i)
        {
            const double distance = i * .25 - .6;
            double a = 0;
            for (const auto& layer : layers)
            {
                const auto sigma = layer[0] * (.65 + .35 * e);
                const auto coverage = sigma == 0 ? (std::abs(distance) <= layer[1] * .5 ? 1.0 : 0.0)
                    : .5 * (std::erf((distance + layer[1] * .5) / (sigma * std::sqrt(2.0)))
                            - std::erf((distance - layer[1] * .5) / (sigma * std::sqrt(2.0))));
                const auto opacity = std::min(1.0, .025 + std::pow(e, .7) * layer[2]);
                a = 1 - (1 - a) * (1 - coverage * opacity);
            }
            // Fade the final pixels to zero rather than exposing a rectangular
            // edge where the transparent auxiliary window ends.
            const auto fade = juce::jlimit(0.0, 1.0, (padding - i * .25) / 8.0);
            alpha[i] = static_cast<float>(a * fade);
        }
        juce::Image::BitmapData data(frame, juce::Image::BitmapData::writeOnly);
        for (const auto& p : pixels)
        {
            auto position = p.angle - phase;
            if (position < 0) position += 1;
            position *= 6;
            const int index = juce::jlimit(0, 5, static_cast<int>(position));
            const auto colour = colours[index].interpolatedWith(colours[index+1], static_cast<float>(position-index));
            const auto coverage = juce::jlimit(0.0f, 1.0f, p.distance + .5f);
            data.setPixelColour(p.x, p.y, colour.withAlpha(coverage * alpha[juce::jlimit(0,192,juce::roundToInt(p.distance*4))]));
        }
    }
};
