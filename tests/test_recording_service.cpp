#include "RecordingService.h"
#include "RecordingPaths.h"
#include "AccompanimentAudioService.h"
#include "VideoAudioExtractor.h"
#include "VideoPlaybackCache.h"
#include "MasterOutputService.h"
#include "ProcessorChangeGate.h"
#include <cassert>
#include <algorithm>
#include <cmath>
#include <thread>
#include <iostream>
#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

int main(int argc,char** argv)
{
    // The host post-FX must not change its sound merely because the device
    // delivers the same samples in 128-, 256-, or irregular-sized blocks.
    {
        constexpr int length=48000;
        auto render=[](const std::vector<int>& blocks) {
            fengyin::MasterOutputService output;
            fengyin::ToneStyleSettings style;
            style.reverbMix=.13f;style.outputGain=1.778279f;
            style.air=.35f;style.bass=.2f;
            output.setToneStyle(style);output.setSampleRate(48000);
            juce::AudioBuffer<float> audio(2,length);
            for(int i=0;i<length;++i) {
                const float sample=i<24000?.15f*std::sin(i*.057f):0.f;
                audio.setSample(0,i,sample);audio.setSample(1,i,sample*.8f);
            }
            int position=0;size_t index=0;
            while(position<length) {
                const int count=std::min(blocks[index++%blocks.size()],length-position);
                float* channels[]{audio.getWritePointer(0,position),audio.getWritePointer(1,position)};
                output.processInstrument(channels,2,count);
                output.processMaster(channels,2,count);position+=count;
            }
            return audio;
        };
        const auto reference=render({128});
        for(const auto& blocks:std::vector<std::vector<int>>{{256},{512},{128,256,128,512},{31,144,240,257}}) {
            const auto actual=render(blocks);
            for(int c=0;c<2;++c)for(int i=0;i<length;++i) {
                assert(std::isfinite(actual.getSample(c,i)));
                assert(std::abs(actual.getSample(c,i)-reference.getSample(c,i))<1e-6f);
            }
        }
    }
    {
        const auto previous=juce::FloatVectorOperations::areDenormalsDisabled();
        juce::FloatVectorOperations::disableDenormalisedNumberSupport(false);
        {
            const juce::ScopedNoDenormals guard;
            assert(juce::FloatVectorOperations::areDenormalsDisabled());
            volatile float a=.125f,b=1e-12f;
            const float normal=a*b;
            assert(normal==.125f*1e-12f);
        }
        assert(!juce::FloatVectorOperations::areDenormalsDisabled());
        juce::FloatVectorOperations::disableDenormalisedNumberSupport(previous);
    }
    {
        fengyin::ProcessorChangeGate gate;std::atomic<bool> changing{false};
        std::thread loader([&]{gate.change([&]{changing.store(true);juce::Thread::sleep(400);});});
        for(int i=0;i<1000&&!changing.load();++i)juce::Thread::sleep(1);
        assert(changing.load());
        const auto started=juce::Time::getMillisecondCounterHiRes();
        bool called=false;
        assert(!gate.processIfAvailable([&]{called=true;}));
        assert(!called && juce::Time::getMillisecondCounterHiRes()-started<150);
        loader.join();
        assert(gate.processIfAvailable([&]{called=true;}) && called);
    }
    // The producer deliberately stalls on disk/decoder work. The consumer must
    // return a silent cache miss instead of waiting behind that work.
    struct SlowReader final:juce::AudioFormatReader {
        std::atomic<bool> entered{false};
        SlowReader():AudioFormatReader(nullptr,"slow-test") {
            sampleRate=48000;numChannels=2;lengthInSamples=480000;
            usesFloatingPointData=true;bitsPerSample=32;
        }
        bool readSamples(int* const* dest,int channels,int offset,juce::int64,int count) override {
            entered.store(true);juce::Thread::sleep(400);
            for(int c=0;c<channels;++c)if(dest[c])juce::FloatVectorOperations::clear(reinterpret_cast<float*>(dest[c])+offset,count);
            return true;
        }
    };
    {
        juce::TimeSliceThread thread("slow-reader-test");thread.startThread();
        auto* slow=new SlowReader;
        juce::BufferingAudioReader buffered(slow,thread,262144);buffered.setReadTimeout(0);
        for(int i=0;i<1000&&!slow->entered.load();++i)juce::Thread::sleep(1);
        assert(slow->entered.load());
        juce::AudioBuffer<float> missed(2,128);missed.clear();
        const auto begin=juce::Time::getMillisecondCounterHiRes();
        assert(!buffered.read(&missed,0,128,0,true,true));
        assert(juce::Time::getMillisecondCounterHiRes()-begin<150);
        assert(missed.getMagnitude(0,128)==0);
    }
    struct TestLogger:juce::Logger {
        void logMessage(const juce::String& text) override {std::cerr<<text<<std::endl;}
        ~TestLogger() override {juce::Logger::setCurrentLogger(nullptr);}
    } diagnostic;
    if(argc==3)juce::Logger::setCurrentLogger(&diagnostic);
    fengyin::MasterOutputService master;
    master.setGain(0.5f);
    master.setSampleRate(48000.0);
    juce::AudioBuffer<float> analysisAudio(2, 4096);
    for (int i = 0; i < analysisAudio.getNumSamples(); ++i)
        analysisAudio.setSample(0, i, std::sin(static_cast<float>(i) * 0.13f));
    analysisAudio.copyFrom(1, 0, analysisAudio, 0, 0, analysisAudio.getNumSamples());
    master.process(analysisAudio.getArrayOfWritePointers(), 2, analysisAudio.getNumSamples());
    assert(master.getLeftPeak() > 0.99f && master.getLeftPeak() <= 1.0f);
    std::array<float, fengyin::MasterOutputService::spectrumBands> spectrum {};
    assert(master.getSpectrum(spectrum));
    assert(*std::max_element(spectrum.begin(), spectrum.end()) > 0.2f);
    juce::AudioBuffer<float> limited(2, 64);
    limited.clear();
    limited.setSample(0, 0, 2.0f);
    limited.setSample(1, 0, -2.0f);
    master.process(limited.getArrayOfWritePointers(), 2, limited.getNumSamples());
    assert(limited.getSample(0, 0) == 2.0f && limited.getSample(1, 0) == -2.0f);

    master.setGain(1.0f);
    master.setReverbMix(0.0f);
    master.setInstrumentProfile(fengyin::InstrumentMixProfile::saxophone);
    juce::AudioBuffer<float> performance(2, 8192);
    performance.clear();
    for (int i = 0; i < performance.getNumSamples(); ++i)
    {
        performance.setSample(0, i, 0.38f);
        performance.setSample(1, i, 0.38f);
    }
    master.processInstrument(performance.getArrayOfWritePointers(), 2, performance.getNumSamples());
    assert(performance.getMagnitude(0, performance.getNumSamples()) > 0.0f);

    // 伴奏在正常电平下经过总输出时必须保持透明：不随乐器活动降低，
    // 也不再经过总线混响或带释放时间的压限。
    juce::AudioBuffer<float> transparentMaster(2, 256);
    transparentMaster.clear();
    for (int channel = 0; channel < transparentMaster.getNumChannels(); ++channel)
        for (int sample = 0; sample < transparentMaster.getNumSamples(); ++sample)
            transparentMaster.setSample(channel, sample, 0.25f);
    master.processMaster(transparentMaster.getArrayOfWritePointers(), 2, transparentMaster.getNumSamples());
    assert(std::abs(transparentMaster.getSample(0, 0) - 0.25f) < 0.000001f);
    assert(std::abs(transparentMaster.getSample(1, 200) - 0.25f) < 0.000001f);

    const auto folder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                            .getChildFile("fengyin-recording-test-" + juce::Uuid().toString());
    const auto file = folder.getChildFile("test.wav");
    const auto namedFirst = fengyin::RecordingService::nextRecordingFile(folder, juce::String::fromUTF8("高音萨克斯"), "20260911");
    assert(namedFirst.getFileName() == juce::String::fromUTF8("风吟录制-260911-1.wav"));
    assert(namedFirst.create().wasOk());
    const auto namedSecond = fengyin::RecordingService::nextRecordingFile(folder, juce::String::fromUTF8("二胡"), "20260911");
    assert(namedSecond.getFileName() == juce::String::fromUTF8("风吟录制-260911-2.wav"));
    assert(fengyin::RecordingService::nextRecordingFile(folder, {}, "261007").getFileName()
        == juce::String::fromUTF8("风吟录制-261007-1.wav"));
    const auto namedVideo=fengyin::RecordingPaths::next(folder,"mp4","20260911");
    assert(namedVideo.getFileName()==juce::String::fromUTF8("风吟录制-260911-1.mp4"));
    assert(namedVideo.withFileExtension(".partial.mp4").create().wasOk());
    assert(fengyin::RecordingPaths::next(folder,"mp4","20260911").getFileName()
        ==juce::String::fromUTF8("风吟录制-260911-2.mp4"));
    fengyin::RecordingService recorder;
    assert(recorder.startToFile(file, 48000.0, 2));
    juce::AudioBuffer<float> audio(2, 4800);
    for (int i = 0; i < audio.getNumSamples(); ++i)
    {
        const auto value = 0.2f * std::sin(static_cast<float>(i) * 0.03f);
        audio.setSample(0, i, value);
        audio.setSample(1, i, -value);
    }
    recorder.push(audio.getArrayOfWritePointers(), 2, audio.getNumSamples());
    recorder.stop();
    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    auto reader = std::unique_ptr<juce::AudioFormatReader>(formats.createReaderFor(file));
    assert(reader != nullptr);
    assert(reader->numChannels == 2);
    assert(reader->lengthInSamples == 4800);
    assert(reader->sampleRate == 48000.0);
    const auto video = folder.getChildFile("score.mp4");
    assert(video.create().wasOk());
    const auto cacheA = fengyin::VideoAudioExtractor::cacheFileFor(video, folder.getChildFile("cache"));
    const auto cacheB = fengyin::VideoAudioExtractor::cacheFileFor(video, folder.getChildFile("cache"));
    assert(cacheA == cacheB && cacheA.hasFileExtension("wav"));
    const auto proxyA=fengyin::VideoPlaybackCache::cacheFileFor(video,folder);
    assert(proxyA.hasFileExtension("mp4") && proxyA.getFileName().startsWith("v1-"));
    assert(proxyA==fengyin::VideoPlaybackCache::cacheFileFor(video,folder));
    assert(proxyA!=fengyin::VideoPlaybackCache::cacheFileFor(file,folder));
    auto metadata=juce::JSON::parse(R"({"streams":[{"codec_name":"h264","width":3840,"height":2160,"avg_frame_rate":"60000/1001"}],"format":{"duration":"120.0"}})");
    auto info=fengyin::VideoPlaybackCache::readInfo(metadata);
    assert(info.width==3840 && std::abs(info.fps-59.94005994)<.0001);
    assert(fengyin::VideoPlaybackCache::worthwhile(info));
    auto hdr=metadata;hdr=juce::JSON::parse(R"({"streams":[{"codec_name":"hevc","width":3840,"height":2160,"avg_frame_rate":"60/1","color_transfer":"smpte2084","color_primaries":"bt2020","pix_fmt":"yuv420p10le"}],"format":{"duration":"120"}})");
    assert(!fengyin::VideoPlaybackCache::worthwhile(fengyin::VideoPlaybackCache::readInfo(hdr)));
    info.width=1920;info.height=1080;info.fps=24;assert(fengyin::VideoPlaybackCache::worthwhile(info));
    info.width=1280;info.height=720;assert(!fengyin::VideoPlaybackCache::worthwhile(info));
    info.fps=60;info.duration=3600;assert(!fengyin::VideoPlaybackCache::worthwhile(info));
    info=fengyin::VideoPlaybackCache::readInfo(juce::JSON::parse("{}"));
    assert(!fengyin::VideoPlaybackCache::worthwhile(info));
    const auto encode=fengyin::VideoPlaybackCache::encodeArguments(folder.getChildFile("ffmpeg"),video,proxyA);
    assert(encode.contains("-an") && encode.contains("-nostdin") && encode.contains("1"));
    assert(encode.joinIntoString(" ").contains("min(source_fps,24)"));
    const auto sidecar = folder.getChildFile("score.wav");
    assert(file.copyFileTo(sidecar));
    fengyin::AccompanimentAudioService accompaniment;
    // A readable WAV disguised as a video must not bypass PCM preparation.
    const auto disguisedVideo=folder.getChildFile("native-readable.mp4");
    assert(file.copyFileTo(disguisedVideo));
    assert(!accompaniment.loadForVideo(disguisedVideo));
    assert(accompaniment.loadForVideo(video));
    assert(accompaniment.getAudioFile()==sidecar);
    accompaniment.prepare(48000.0, 512);
    accompaniment.play();
    juce::AudioBuffer<float> mixed(2, 512);
    for (int tries = 0; tries < 20 && mixed.getMagnitude(0, mixed.getNumSamples()) == 0.0f; ++tries)
    {
        mixed.clear();
        juce::Thread::sleep(5);
        accompaniment.mixInto(mixed.getArrayOfWritePointers(), 2, mixed.getNumSamples());
    }
    assert(mixed.getMagnitude(0, mixed.getNumSamples()) > 0.0f);
    assert(accompaniment.getReadMissCount()==0);

    accompaniment.release();
    accompaniment.unload();
    if(argc==3) {
        juce::ScopedJuceInitialiser_GUI init;
        const juce::File ffmpeg(juce::String::fromUTF8(argv[1])),source(juce::String::fromUTF8(argv[2]));
        const auto original=juce::SHA256(source).toHexString();
        fengyin::VideoPlaybackCache cache(folder.getChildFile("proxy-cache"));
        juce::File first;int passes=0;
        std::function<void()> launch;
        launch=[&] {
            const bool started=cache.prepare(ffmpeg,source,[&](const juce::File& result){
            if(!result.existsAsFile()) {juce::MessageManager::getInstance()->stopDispatchLoop();return;}
            if(passes==0)first=result;else assert(result==first);
            assert(juce::SHA256(source).toHexString()==original);
            assert(result.getSize()>64 && result.getSize()<512LL*1024*1024);
            if(++passes==2)juce::MessageManager::getInstance()->stopDispatchLoop();
            else juce::Timer::callAfterDelay(50,launch);
            });
            if(!started){std::cerr<<"Could not start video cache task\n";juce::MessageManager::getInstance()->stopDispatchLoop();}
        };
        juce::Timer::callAfterDelay(30000,[]{juce::MessageManager::getInstance()->stopDispatchLoop();});
        launch();
       #if JUCE_MAC
        const auto until=juce::Time::getMillisecondCounterHiRes()+31000;
        while(passes<2 && !juce::MessageManager::getInstance()->hasStopMessageBeenSent()
              && juce::Time::getMillisecondCounterHiRes()<until)
            CFRunLoopRunInMode(kCFRunLoopDefaultMode,.01,false);
       #else
        juce::MessageManager::getInstance()->runDispatchLoop();
       #endif
        if(passes!=2){cache.interrupt();std::cerr<<"Video proxy preparation failed; passes="<<passes;return 50;}
        // Keep a verified output for external decoding before test cleanup.
        assert(first.copyFileTo(source.getSiblingFile("verified-proxy.mp4")));
        std::cout<<"PASS native idle video proxy, reusable cache, original unchanged; external decode required\n";
    }
    folder.deleteRecursively();
    return 0;
}
