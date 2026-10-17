#pragma once
#include <juce_cryptography/juce_cryptography.h>
#include <juce_events/juce_events.h>
#include <functional>

namespace fengyin {
// Only prepares the selected clip while the host is idle. The source and its
// separately extracted audio are never modified. A failed proxy is optional.
class VideoPlaybackCache {
public:
    using Completion=std::function<void(const juce::File&)>;
    explicit VideoPlaybackCache(juce::File directory={}) : cacheDirectory(std::move(directory)) {}
    ~VideoPlaybackCache();
    bool prepare(const juce::File& ffmpeg,const juce::File& source,Completion);
    void interrupt(); // non-blocking; the child is killed by its worker
    bool busy() const {return pool.getNumJobs()>0;}
    static juce::File cacheFileFor(const juce::File&,const juce::File& directory);
    static juce::StringArray encodeArguments(const juce::File& ffmpeg,const juce::File& source,
                                            const juce::File& destination,bool hardware=true);
    struct Info {int width=0,height=0;double fps=0,duration=0;juce::String codec;bool standardDynamicRange=true;};
    static Info readInfo(const juce::var&);
    static bool worthwhile(const Info&);
private:
    juce::ThreadPool pool{1,juce::Thread::osDefaultStackSize,juce::Thread::Priority::low};
    juce::File cacheDirectory;
};
}
