#pragma once
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <juce_gui_extra/juce_gui_extra.h>
#include "VideoHostRendering.h"
#include "NativeVideoFileDialog.h"
#if JUCE_WINDOWS
 #include <windows.h>
#endif

// Post both entry and completion out of WebView callbacks. Windows' native
// picker lives in an isolated process so its Shell activation cannot stall us.
class VideoFileChooser final : public juce::Component, private juce::Timer
{
public:
    explicit VideoFileChooser(juce::Component& parent, juce::File helper = {})
       #if JUCE_WINDOWS
        : native(std::move(helper)), owner(&parent) {}
       #else
        : owner(&parent) { juce::ignoreUnused(helper); }
       #endif
    ~VideoFileChooser() override { stopTimer(); }
    bool isPending() const noexcept { return pending; }
    void open(std::function<void(juce::File)> completion, std::function<void(juce::String)> error = {})
    {
        if (pending) {
           #if JUCE_WINDOWS
            native.bringToFront();
           #endif
            return;
        }
        pending = true;
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<VideoFileChooser>(this), completion = std::move(completion), error = std::move(error)]() mutable
        {
            if (safe == nullptr) return;
            if (safe->owner == nullptr) { safe->pending = false; return; }
           #if JUCE_WINDOWS
            safe->onComplete = std::move(completion);
            safe->onError = std::move(error);
            safe->terminalAt = 0;
            RECT bounds {};
            if (auto* peer = safe->owner->getPeer()) GetWindowRect(static_cast<HWND>(peer->getNativeHandle()), &bounds);
            if (!safe->native.start(bounds)) { safe->finish({}, true); return; }
            safe->startTimer(50);
           #else
            juce::ignoreUnused(error);
            // A non-null JUCE parent makes FileChooserDialogBox a painted
            // child with no desktop peer. WebView2's native child HWND then
            // covers the modal UI. Use a separate window, not IFileDialog.
            safe->chooser = std::make_unique<juce::FileChooser>(juce::String::fromUTF8("选择伴奏视频"),
                safe->lastFolder, "*.mp4;*.mov;*.m4v;*.avi;*.mkv;*.webm", false, false, nullptr);
            safe->chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                [safe, completion = std::move(completion)](const juce::FileChooser& picker) mutable
                {
                    const auto result = picker.getResult();
                    juce::MessageManager::callAsync([safe, result, completion = std::move(completion)]() mutable
                    {
                        if (safe == nullptr) return;
                        safe->chooser.reset();
                        safe->pending = false;
                        if (result.existsAsFile()) safe->lastFolder = result.getParentDirectory();
                        completion(result);
                    });
                });
            if(auto* dialog=juce::Component::getCurrentlyModalComponent()) {
                fengyin::configureVideoHostRendering(dialog->getPeer());
                if(auto* display=juce::Desktop::getInstance().getDisplays().getDisplayForRect(safe->owner->getScreenBounds())) {
                    const auto area=display->userBounds.toNearestInt().reduced(8);
                    const auto bounds=juce::Rectangle<int>(juce::jmin(600,area.getWidth()),juce::jmin(500,area.getHeight()))
                        .withCentre(safe->owner->getScreenBounds().getCentre()).constrainedWithin(area);
                    dialog->setBounds(bounds);
                }
                dialog->toFront(true);
                dialog->grabKeyboardFocus();
            }
           #endif
        });
    }
   #if JUCE_WINDOWS
    HWND nativeWindow() { return native.window(); }
    DWORD nativeProcessId() const { return native.processId(); }
    juce::var nativeDiagnostics() { return native.diagnostics(); }
   #endif
private:
    void timerCallback() override
    {
       #if JUCE_WINDOWS
        const auto result = native.result();
        if (result.isObject()) {
            // Give a healthy Shell time to persist its last folder and exit.
            // An unhealthy closing/activation path is bounded, never waited on.
            if (terminalAt == 0) terminalAt = juce::Time::getMillisecondCounterHiRes();
            if (native.running() && juce::Time::getMillisecondCounterHiRes() - terminalAt < 250) return;
            const auto status = result["status"].toString();
            const auto path = result["value"].toString();
            const auto file = status == "selected" && juce::File::isAbsolutePath(path) ? juce::File(path) : juce::File();
            finish(file, (status != "selected" && status != "cancelled") || (status == "selected" && !file.existsAsFile()));
            return;
        }
        native.serviceForegroundRequest();
        if (!native.running() || native.startupExpired()) finish({}, true);
       #endif
    }
   #if JUCE_WINDOWS
    fengyin::NativeVideoDialogSession native;
    std::function<void(juce::File)> onComplete;
    std::function<void(juce::String)> onError;
    double terminalAt = 0;
    void finish(juce::File file, bool failed)
    {
        stopTimer(); native.close(); pending = false;
        auto callback = std::move(onComplete);
        auto error = std::move(onError);
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<VideoFileChooser>(this), callback = std::move(callback), error = std::move(error), file, failed]() mutable
        {
            if (!safe || !safe->owner) return;
            if (failed) {
                juce::Logger::writeToLog("Native video selector failed; existing video retained");
                if (error) error(juce::String::fromUTF8("无法打开视频选择窗口，请重试。原视频保持不变。"));
            } else if (callback) callback(file);
        });
    }
   #endif
    juce::Component::SafePointer<juce::Component> owner;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastFolder;
    bool pending = false;
};
