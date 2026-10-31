#pragma once
#ifndef NOMINMAX
 #define NOMINMAX
#endif
#include <juce_gui_extra/juce_gui_extra.h>
#include "VideoHostRendering.h"
#if JUCE_WINDOWS
 #include <windows.h>
#endif

// Video selection deliberately avoids IFileDialog. The captured Honor hang is
// inside its closing/activation path, before VLC was loaded. Post both entry
// and completion out of WebView callbacks; never run a nested modal loop there.
class VideoFileChooser final : public juce::Component
{
public:
    explicit VideoFileChooser(juce::Component& parent) : owner(&parent) {}
    bool isPending() const noexcept { return pending; }
    void open(std::function<void(juce::File)> completion)
    {
        if (pending) return;
        pending = true;
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<VideoFileChooser>(this), completion = std::move(completion)]() mutable
        {
            if (safe == nullptr || safe->owner == nullptr) return;
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
               #if JUCE_WINDOWS
                if(dialog->getPeer()!=nullptr && safe->owner->getPeer()!=nullptr) {
                    // GWLP_HWNDPARENT assigns an OWNER for a top-level HWND.
                    // Do not SetParent/WS_CHILD: that would reintroduce airspace.
                    const auto window=static_cast<HWND>(dialog->getPeer()->getNativeHandle());
                    const auto host=static_cast<HWND>(safe->owner->getPeer()->getNativeHandle());
                    SetWindowLongPtrW(window,GWLP_HWNDPARENT,reinterpret_cast<LONG_PTR>(host));
                }
               #endif
                dialog->toFront(true);
                dialog->grabKeyboardFocus();
            }
        });
    }
private:
    juce::Component::SafePointer<juce::Component> owner;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastFolder;
    bool pending = false;
};
