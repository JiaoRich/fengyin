#include <juce_audio_devices/juce_audio_devices.h>
#include "RealtekAsioConfiguration.h"
#include <iostream>
#include <atomic>

struct SilentCallback final : juce::AudioIODeviceCallback
{
    std::atomic<int> calls{0};
    void audioDeviceIOCallbackWithContext(const float* const*, int, float* const* out,
        int channels, int frames, const juce::AudioIODeviceCallbackContext&) override
    {
        for (int i=0;i<channels;++i) if(out[i]) juce::FloatVectorOperations::clear(out[i], frames);
        ++calls;
    }
    void audioDeviceAboutToStart(juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}
};
int main(int argc, char** argv)
{
    if (argc!=3) return 2;
    const juce::String mode(argv[1]);
    if (mode!="preserve" && mode!="read" && mode!="configure" && mode!="configure-reset") return 2;
    juce::ScopedJuceInitialiser_GUI gui;
    juce::FileLogger logger(juce::File::getCurrentWorkingDirectory().getChildFile(argv[2]),
        "ASIO startup comparison; silent callbacks are NOT an audible playback test.", 0);
    juce::Logger::setCurrentLogger(&logger);
    const juce::ScopeGuard clearLogger{[]{juce::Logger::setCurrentLogger(nullptr);}};
    juce::Logger::writeToLog("MODE="+mode);
    std::unique_ptr<juce::AudioIODeviceType> type(juce::AudioIODeviceType::createAudioIODeviceType_ASIO());
    if(!type) return 3;
    type->scanForDevices();
    juce::String driver;
    for(auto name:type->getDeviceNames(false)) if(name.containsIgnoreCase("ASIO4ALL")){driver=name;break;}
    if(driver.isEmpty()) return 4;
    SilentCallback callback;
    std::unique_ptr<juce::AudioIODevice> device(type->createDevice(driver,{}));
    if(!device) return 5;
    device->close();
    if(mode!="preserve")
    {
        const auto result=fengyin::audioengine::configureRealtekOutputs(device->getFengYinAsioInterface(),mode=="read");
        juce::Logger::writeToLog("configuration="+juce::String(result.configured?1:0));
        if(!result.rollbackOK) return 6;
    }
    if(mode=="configure-reset" && !device->requestFengYinAsioReinitialisation()) return 7;
    juce::BigInteger outputs;outputs.setRange(0,2,true);
    juce::Logger::writeToLog("OPEN 48000Hz 128 frames stereo");
    const auto error=device->open({},outputs,48000,128);
    if(error.isNotEmpty()){juce::Logger::writeToLog("OPEN FAILED="+error);return 8;}
    juce::Logger::writeToLog("CHANNELS="+device->getOutputChannelNames().joinIntoString(","));
    device->start(&callback);
    juce::Thread::sleep(2000);
    device->stop();device->close();
    juce::Logger::writeToLog("CALLBACKS="+juce::String(callback.calls.load()));
    juce::Logger::writeToLog("CLEAN STOP; silence only, not full product acceptance");
    return callback.calls.load()>10?0:9;
}
