#pragma once
#include <juce_audio_utils/juce_audio_utils.h>

namespace fengyin {
// Separate output callback: prompts are mixed by AudioDeviceManager, downstream
// of both instrument/recording callbacks, and are never passed to the recorder.
class VoicePromptService final : public juce::AudioIODeviceCallback {
public:
    VoicePromptService() { formats.registerBasicFormats(); }
    bool play(const void* bytes, int size, int request, float gain) {
        auto input=std::make_unique<juce::MemoryInputStream>(bytes,static_cast<size_t>(size),false);
        std::unique_ptr<juce::AudioFormatReader> reader(formats.createReaderFor(std::move(input)));
        if (!reader || reader->lengthInSamples<=0 || reader->lengthInSamples>24000*120) return false;
        juce::AudioBuffer<float> next(1,static_cast<int>(reader->lengthInSamples));
        if (!reader->read(&next,0,next.getNumSamples(),0,true,false)) return false;
        const juce::SpinLock::ScopedLockType guard(lock);
        buffer=std::move(next);sourceRate=reader->sampleRate;position=0;
        volume=juce::jlimit(0.f,.7f,gain);token=request;finished.store(0);return true;
    }
    void stop() {const juce::SpinLock::ScopedLockType guard(lock);token=0;}
    int consumeFinished() noexcept {return finished.exchange(0);}
    void audioDeviceAboutToStart(juce::AudioIODevice* device) override {rate=device?device->getCurrentSampleRate():48000;}
    void audioDeviceStopped() override {stop();}
    void audioDeviceIOCallbackWithContext(const float* const*,int,float* const* outputs,int channels,int samples,const juce::AudioIODeviceCallbackContext&) override {
        for(int c=0;c<channels;++c)if(outputs[c])juce::FloatVectorOperations::clear(outputs[c],samples);
        const juce::SpinLock::ScopedTryLockType guard(lock);
        if(!guard.isLocked() || token==0 || rate<=0)return;
        const auto* data=buffer.getReadPointer(0);const int length=buffer.getNumSamples();
        for(int i=0;i<samples;++i) {
            if(position>=length){finished.store(token);token=0;break;}
            const int a=static_cast<int>(position),b=juce::jmin(a+1,length-1);
            const auto fraction=static_cast<float>(position-a);
            const float value=(data[a]+fraction*(data[b]-data[a]))*volume;
            for(int c=0;c<channels;++c)if(outputs[c])outputs[c][i]=value;
            position+=sourceRate/rate;
        }
    }
private:
    juce::AudioFormatManager formats;
    juce::AudioBuffer<float> buffer;
    juce::SpinLock lock;
    double rate=48000,sourceRate=24000,position=0;
    float volume=.45f;int token=0;std::atomic<int> finished{0};
};
}
