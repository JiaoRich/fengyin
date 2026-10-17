#include <juce_gui_extra/juce_gui_extra.h>
#include "MainComponent.h"
#include "PluginScanWorker.h"
#if JUCE_WINDOWS
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <dwmapi.h>
#endif
#include "CompactWindowGlow.h"
#include "NativeCompactDragSurface.h"

class FengYinApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return juce::String::fromUTF8("风吟"); }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override
    {
        return juce::JUCEApplication::getCommandLineParameters().startsWith("--scan-plugin ");
    }

    void initialise(const juce::String& commandLine) override
    {
        auto arguments = juce::StringArray::fromTokens(commandLine, true);
        arguments.removeEmptyStrings();
        if (arguments.size() == 3 && arguments[0] == "--scan-plugin")
        {
            scanWorker = std::make_unique<PluginScanWorker>(arguments[1].unquoted(), juce::File(arguments[2].unquoted()));
            return;
        }
        logger.reset(juce::FileLogger::createDefaultAppLogger("FengYin", "plugin-host.log",
            "FengYin " + getApplicationVersion() + " started"));
        juce::Logger::setCurrentLogger(logger.get());
        juce::Logger::writeToLog("BUILD revision=" FENGYIN_BUILD_REVISION " onset=2 ordered-midi=1");
        juce::Logger::writeToLog("BUILD executable=" + juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName());
        mainWindow = std::make_unique<MainWindow>(getApplicationName());
    }

    void shutdown() override
    {
        scanWorker.reset(); mainWindow.reset();
        juce::Logger::setCurrentLogger(nullptr); logger.reset();
    }
    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow final : public juce::DocumentWindow, private juce::Timer
    {
    public:
        explicit MainWindow(juce::String name)
            : DocumentWindow(std::move(name), juce::Colour(0xff07101d), allButtons)
        {
            // Keep the native peer stable for the lifetime of WebView2.
            setUsingNativeTitleBar(false);
            setTitleBarHeight(0);
            content = new MainComponent();
            setContentOwned(content, true);
            content->onWindowModeChanged = [this](bool compact) { applyMode(compact); };
            content->onWindowDragRegionChanged = [this](juce::Rectangle<int> region) {
               #if JUCE_WINDOWS
                dragRegion=region;updateNativeDragSurface();
               #else
                juce::ignoreUnused(region);
               #endif
            };
            content->onWindowAction = [this](const juce::String& action)
            {
                if (action == "close") closeButtonPressed();
                else if (action == "minimise") setMinimised(true);
                else if (action == "pointer-end") { pointerGesture = false; windowGesture.clear(); }
                else if (content->compactMode && action.startsWith("pointer-begin:"))
                {
                    windowGesture = action.fromFirstOccurrenceOf(":", false, false);
                    pointerGesture = true; gestureBounds = getBounds();
                    capturePhysicalGesture();
                }
                else if (pointerGesture && content->compactMode && action.startsWith("pointer-move:"))
                {
                    const auto parts = juce::StringArray::fromTokens(action, ":", "");
                    if (parts.size() == 3) applyWindowGestureDelta({parts[1].getIntValue(),parts[2].getIntValue()});
                }
                else if (content->compactMode && (action == "drag" || action.startsWith("resize-")))
                {
                    // WebView owns mouse capture. Track desktop coordinates rather
                    // than starting a delayed non-client loop on a borderless HWND.
                    windowGesture = action;
                    pointerGesture = false;
                    gestureStart = juce::Desktop::getMousePosition();
                    gestureBounds = getBounds();
                    capturePhysicalGesture();
                }
            };
            setResizable(false, false);
            setResizeLimits(1120, 700, 2560, 1600);
            const auto display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay();
            const auto available = display != nullptr ? display->userBounds.toNearestInt()
                                                      : juce::Rectangle<int>(0, 0, 1366, 768);
            centreWithSize(juce::jmin(1440, juce::jmax(1120, available.getWidth() - 32)),
                           juce::jmin(900, juce::jmax(700, available.getHeight() - 32)));
            setVisible(true);
            setFullScreen(true);
            startTimerHz(60); // Input polling only; the visual companion remains capped at 24 Hz.
        }

        void maximiseButtonPressed() override { content->requestCompactMode(!content->compactMode); }
        bool keyPressed(const juce::KeyPress& key) override
        {
            if (key == juce::KeyPress::escapeKey && isFullScreen())
            { content->requestCompactMode(true); return true; }
            return DocumentWindow::keyPressed(key);
        }

        void applyMode(bool compact)
        {
            pointerGesture = false; windowGesture.clear();
            borderless = compact;
            roundedWidth = -1;
            if (! compact) { updateRoundedWindow(false); setResizeLimits(1120, 700, 7680, 4320); setFullScreen(true); return; }
            const auto* display = juce::Desktop::getInstance().getDisplays().getDisplayForRect(getBounds());
            const auto area = display != nullptr ? display->userBounds.toNearestInt() : juce::Rectangle<int>(0, 0, 1366, 768);
            setResizeLimits(480, 560, 960, 2160);
            setFullScreen(false);
            const auto safeArea = area.reduced(12);
            const int width = juce::jmin(safeArea.getWidth(), juce::jlimit(480, 680, area.getWidth() * 2 / 5));
            setBounds(safeArea.getRight() - width, safeArea.getY(), width, safeArea.getHeight());
            updateRoundedWindow(true);
        }

        juce::BorderSize<int> getBorderThickness() const override
        { return borderless ? juce::BorderSize<int>(0) : DocumentWindow::getBorderThickness(); }

        void closeButtonPressed() override
        {
            juce::JUCEApplication::getInstance()->systemRequestedQuit();
        }
    private:
       #if JUCE_WINDOWS
        NativeCompactDragSurface nativeDrag;
        juce::Rectangle<int> dragRegion;
        void updateNativeDragSurface() {
            if(auto* peer=getPeer()) {
                const auto scale=peer->getPlatformScaleFactor();
                nativeDrag.update(static_cast<HWND>(peer->getNativeHandle()),
                    juce::roundToInt(dragRegion.getX()*scale),juce::roundToInt(dragRegion.getY()*scale),
                    juce::roundToInt(dragRegion.getWidth()*scale),juce::roundToInt(dragRegion.getHeight()*scale),
                    content->compactMode&&!isFullScreen()&&!isMinimised()&&isVisible());
            }
        }
       #endif
        juce::String windowGesture;
        bool pointerGesture = false;
        juce::Point<int> gestureStart;
        juce::Rectangle<int> gestureBounds;
       #if JUCE_WINDOWS
        RECT gesturePhysicalBounds {};
        POINT gesturePhysicalStart {};
        double gestureScale = 1.0;
       #endif
        void capturePhysicalGesture()
        {
           #if JUCE_WINDOWS
            if (auto* peer=getPeer())
            {
                auto hwnd=static_cast<HWND>(peer->getNativeHandle());
                GetWindowRect(hwnd,&gesturePhysicalBounds);
                GetCursorPos(&gesturePhysicalStart);
                gestureScale=peer->getPlatformScaleFactor();
            }
           #endif
        }
        void updateWindowGesture()
        {
            if (windowGesture.isEmpty()) return;
            if (!content->compactMode || isFullScreen() || isMinimised() || !isActiveWindow())
            { pointerGesture=false; windowGesture.clear(); return; }
            if (pointerGesture) return; // Touch/pen uses captured WebView pointer deltas.
           #if JUCE_WINDOWS
            if ((GetAsyncKeyState(VK_LBUTTON)&0x8000)==0)
            {windowGesture.clear();return;}
            POINT position{};
            if(GetCursorPos(&position))applyPhysicalGestureDelta(position.x-gesturePhysicalStart.x,position.y-gesturePhysicalStart.y);
            return;
           #endif
            if (!juce::ModifierKeys::getCurrentModifiersRealtime().isLeftButtonDown())
            { windowGesture.clear(); return; }
            const auto delta = juce::Desktop::getMousePosition() - gestureStart;
            applyWindowGestureDelta(delta);
        }
        void applyWindowGestureDelta(juce::Point<int> delta)
        {
           #if JUCE_WINDOWS
            // CSS screen coordinates are DIPs; the HWND rectangle is physical.
            // Keep the start monitor's scale throughout a gesture.
            applyPhysicalGestureDelta(juce::roundToInt(delta.x*gestureScale),juce::roundToInt(delta.y*gestureScale));
            return;
           #endif
            if (windowGesture == "drag")
            { setBounds(gestureBounds.translated(delta.x, delta.y)); return; }
            const auto edge = windowGesture.fromFirstOccurrenceOf("resize-", false, false);
            auto bounds = gestureBounds;
            if (edge.contains("e")) bounds.setWidth(juce::jlimit(480, 960, gestureBounds.getWidth() + delta.x));
            if (edge.contains("s")) bounds.setHeight(juce::jlimit(560, 2160, gestureBounds.getHeight() + delta.y));
            if (edge.contains("w")) bounds.setLeft(gestureBounds.getRight() - juce::jlimit(480, 960, gestureBounds.getWidth() - delta.x));
            if (edge.contains("n")) bounds.setTop(gestureBounds.getBottom() - juce::jlimit(560, 2160, gestureBounds.getHeight() - delta.y));
            setBounds(bounds);
        }
       #if JUCE_WINDOWS
        void applyPhysicalGestureDelta(int dx,int dy)
        {
            auto* peer=getPeer();if(!peer||windowGesture.isEmpty())return;
            auto bounds=gesturePhysicalBounds;
            const auto edge=windowGesture.fromFirstOccurrenceOf("resize-",false,false);
            if(windowGesture=="drag") {bounds.left+=dx;bounds.right+=dx;bounds.top+=dy;bounds.bottom+=dy;}
            else {
                const int minW=juce::roundToInt(480*gestureScale),maxW=juce::roundToInt(960*gestureScale);
                const int minH=juce::roundToInt(560*gestureScale),maxH=juce::roundToInt(2160*gestureScale);
                if(edge.contains("e"))bounds.right=bounds.left+juce::jlimit<int>(minW,maxW,gesturePhysicalBounds.right-gesturePhysicalBounds.left+dx);
                if(edge.contains("s"))bounds.bottom=bounds.top+juce::jlimit<int>(minH,maxH,gesturePhysicalBounds.bottom-gesturePhysicalBounds.top+dy);
                if(edge.contains("w"))bounds.left=bounds.right-juce::jlimit<int>(minW,maxW,gesturePhysicalBounds.right-gesturePhysicalBounds.left-dx);
                if(edge.contains("n"))bounds.top=bounds.bottom-juce::jlimit<int>(minH,maxH,gesturePhysicalBounds.bottom-gesturePhysicalBounds.top-dy);
            }
            auto hwnd=static_cast<HWND>(peer->getNativeHandle());RECT current{};
            if(GetWindowRect(hwnd,&current)&&EqualRect(&current,&bounds))return;
            SetWindowPos(hwnd,nullptr,bounds.left,bounds.top,bounds.right-bounds.left,bounds.bottom-bounds.top,SWP_NOZORDER|SWP_NOACTIVATE);
        }
       #endif
        void updateRoundedWindow(bool rounded)
        {
           #if JUCE_WINDOWS
            auto* peer = getPeer();
            if (peer == nullptr) return;
            auto hwnd = static_cast<HWND>(peer->getNativeHandle());
            RECT bounds {};
            if (!GetWindowRect(hwnd, &bounds)) return;
            const int width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
            const int radius = juce::roundToInt(28.0 * peer->getPlatformScaleFactor());
            if (rounded == roundedApplied && width == roundedWidth && height == roundedHeight && radius == roundedRadius) return;
            // A GDI window region is a binary mask: it cannot antialias the
            // corners and also prevents Windows 11 DWM rounding. Prefer DWM's
            // composited corners while keeping the WebView host opaque.
            SetWindowRgn(hwnd, nullptr, TRUE);
            const DWORD preference = rounded ? 2u : 1u; // DWMWCP_ROUND / DONOTROUND
            smoothNativeCorners = SUCCEEDED(DwmSetWindowAttribute(hwnd, 33, &preference, sizeof(preference)));
            if (smoothNativeCorners)
            {
                const COLORREF noBorder = 0xfffffffe; // DWMWA_COLOR_NONE
                DwmSetWindowAttribute(hwnd, 34, &noBorder, sizeof(noBorder));
                const MARGINS margins {rounded ? 1 : 0, rounded ? 1 : 0, rounded ? 1 : 0, rounded ? 1 : 0};
                DwmExtendFrameIntoClientArea(hwnd, &margins);
                roundedApplied = rounded; roundedWidth = width; roundedHeight = height; roundedRadius = radius;
                return;
            }
            auto region = rounded ? CreateRoundRectRgn(0, 0, width + 1, height + 1, radius * 2, radius * 2) : nullptr;
            if (rounded && region == nullptr) return;
            if (SetWindowRgn(hwnd, region, TRUE) == 0)
            { if (region != nullptr) DeleteObject(region); return; }
            // Windows owns the region after successful SetWindowRgn.
            roundedApplied = rounded; roundedWidth = width; roundedHeight = height; roundedRadius = radius;
           #else
            juce::ignoreUnused(rounded);
           #endif
        }
        bool roundedApplied = false;
        bool smoothNativeCorners = false;
        bool borderless = false;
        int roundedWidth = -1, roundedHeight = -1, roundedRadius = -1;
        double lastVisualUpdate=0;
        void timerCallback() override
        {
           #if JUCE_WINDOWS
            updateNativeDragSurface();
           #endif
            updateWindowGesture();
            const auto now=juce::Time::getMillisecondCounterHiRes();
            if(now-lastVisualUpdate<1000.0/24.0)return;
            lastVisualUpdate=now;
            content->setWebPresentationVisible(isVisible() && !isMinimised());
            if (!isMinimised()) updateRoundedWindow(content->compactMode && !isFullScreen());
            glow.update(*this, content->compactMode && !isFullScreen() && !isMinimised()
                        && isVisible(), content->getWindowBreath(), smoothNativeCorners ? 8.0f : 28.0f);
            // Native OS restore gestures may bypass maximiseButtonPressed.
            if (! isMinimised() && ! isFullScreen() && ! content->compactMode)
            {
                setFullScreen(true); // Cancelling the video prompt retains the full layout.
                content->requestCompactMode(true);
            }
            else if (isFullScreen() && content->compactMode)
                content->requestCompactMode(false);
        }
        MainComponent* content = nullptr;
        CompactWindowGlow glow;
    };

    std::unique_ptr<MainWindow> mainWindow;
    std::unique_ptr<PluginScanWorker> scanWorker;
    std::unique_ptr<juce::FileLogger> logger;
};

START_JUCE_APPLICATION(FengYinApplication)
