#pragma once
#include <juce_gui_extra/juce_gui_extra.h>
#include "VideoHostRendering.h"

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
            safe->chooser = std::make_unique<juce::FileChooser>(juce::String::fromUTF8("选择伴奏视频"),
                safe->lastFolder, "*.mp4;*.mov;*.m4v;*.avi;*.mkv;*.webm", false, false, safe->owner);
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
            if(auto* dialog=juce::Component::getCurrentlyModalComponent())
                fengyin::configureVideoHostRendering(dialog->getPeer());
        });
    }
private:
    juce::Component::SafePointer<juce::Component> owner;
    std::unique_ptr<juce::FileChooser> chooser;
    juce::File lastFolder;
    bool pending = false;
};
