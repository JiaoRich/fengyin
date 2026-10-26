#include "TonePackage.h"
#include "KongProjectFile.h"
#include "SerialEffectRouting.h"
#include "ScopedGraphPause.h"
#include "ToneRestorePlan.h"
#include "SwamStringTremolo.h"
#include "KensaBendRange.h"
#include "AudioCallbackTrace.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <thread>

class TestEffect final : public juce::AudioProcessor
{
public:
    TestEffect(float multiplier, float offset) : AudioProcessor(BusesProperties()
        .withInput("In", juce::AudioChannelSet::stereo()).withOutput("Out", juce::AudioChannelSet::stereo())),
        gain(multiplier), bias(offset) {}
    const juce::String getName() const override { return "TestEffect"; }
    int prepareCount = 0, releaseCount = 0;
    int preparedBlockSize = 0;
    double preparedRate = 0;
    void prepareToPlay(double rate, int blockSize) override
    { ++prepareCount;preparedRate=rate;preparedBlockSize=blockSize; }
    void releaseResources() override { ++releaseCount; }
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    { for(int c=0;c<buffer.getNumChannels();++c) for(int i=0;i<buffer.getNumSamples();++i) buffer.setSample(c,i,buffer.getSample(c,i)*gain+bias); }
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock& data) override { data.append(&gain,sizeof(gain));data.append(&bias,sizeof(bias)); }
    void setStateInformation(const void*,int) override {}
private:
    float gain, bias;
};

int main()
{
    {
        TestEffect fake(1,0);
        fake.addParameter(new juce::AudioParameterFloat("unused","#000",0.f,1.f,0.f));
        // Match the library's integer-percent range, not JUCE's convenience
        // constructor (which quantizes a 0..1 range to steps of 0.01).
        auto* sensitivity=new juce::AudioParameterFloat("binding1","percent",
            juce::NormalisableRange<float>(10.f,400.f,1.f),100.f);
        auto* other=new juce::AudioParameterFloat("other","#002",0.f,1.f,.42f);
        fake.addParameter(sensitivity);fake.addParameter(other);
        for(int n=1;n<=4;++n){
            assert(fengyin::KensaBendRange::set(fake,n));
            assert(std::abs(sensitivity->get()-n*100.f)<.001f);
            assert(std::abs(other->get()-.42f)<1e-6f);
        }
        assert(!fengyin::KensaBendRange::set(fake,0));
        assert(!fengyin::KensaBendRange::set(fake,5));
        TestEffect unbound(1,0);
        unbound.addParameter(new juce::AudioParameterFloat("unused","#000",0.f,1.f,0.f));
        unbound.addParameter(new juce::AudioParameterFloat("unused1","#001",0.f,1.f,0.f));
        assert(!fengyin::KensaBendRange::set(unbound,1));
    }
    {
        TestEffect fake(1,0);
        auto* selector=new juce::AudioParameterChoice("tremolo","Tremolo",{"OFF","Slow","Fast"},0);
        auto* mode=new juce::AudioParameterChoice("mode","Tremolo Mode",{"Hz","Sync","Sync/Acc"},1);
        auto* speed=new juce::AudioParameterFloat("speed","Tremolo Min Speed",5.f,11.f,8.f);
        auto* pressure=new juce::AudioParameterFloat("bow","Bow Pressure",0.f,1.f,.62f);
        fake.addParameter(selector);fake.addParameter(mode);fake.addParameter(speed);fake.addParameter(pressure);
        fengyin::SwamStringTremolo tremolo;assert(tremolo.prepare(fake));
        assert(mode->getIndex()==0 && std::abs(speed->get()-10.f)<.05f);
        tremolo.request(true);tremolo.flush();assert(selector->getIndex()==1);
        tremolo.setTempoMode(0);tremolo.flush();assert(std::abs(speed->get()-7.f)<.05f);
        tremolo.setTempoMode(2);tremolo.flush();assert(selector->getIndex()==2 && std::abs(speed->get()-140.f/15)<.05f);
        tremolo.request(false);tremolo.flush();assert(selector->getIndex()==0);
        assert(std::abs(pressure->get()-.62f)<1e-6f);
        tremolo.clear();assert(!tremolo.available());
        TestEffect unsupported(1,0);assert(!tremolo.prepare(unsupported));
    }
    const juce::Array<fengyin::ToneParameterValue> baseline {
        fengyin::ToneParameterValue{"timbre", .5f},
        fengyin::ToneParameterValue{"room", .2f},
        fengyin::ToneParameterValue{"unrelated-model", .8f}
    };
    const juce::Array<fengyin::ToneParameterValue> first {
        fengyin::ToneParameterValue{"timbre", .7f},
        fengyin::ToneParameterValue{"room", .4f}
    };
    const juce::Array<fengyin::ToneParameterValue> second {
        fengyin::ToneParameterValue{"timbre", .3f}
    };
    assert(fengyin::retiredToneParameters({}, first, baseline).isEmpty());
    assert(fengyin::retiredToneParameters(first, first, baseline).isEmpty());
    const auto retired = fengyin::retiredToneParameters(first, second, baseline);
    assert(retired.size() == 1 && retired[0].identifier == "room" && retired[0].value == .2f);
    assert(fengyin::retiredToneParameters(second, first, baseline).isEmpty());
    assert(!fengyin::toneValueNeedsWrite(.5f, .5f));
    assert(fengyin::toneValueNeedsWrite(.5f, .6f));
    assert(!fengyin::toneValueNeedsWrite(.5f, 2.0f));
    juce::ScopedJuceInitialiser_GUI init;
    // Current two-slot migrations preserve the original full CH1 and rack;
    // the new secondary slot keeps the same library/mix with its saved technique.
    const auto repository = juce::File(juce::String::fromUTF8(__FILE__)).getParentDirectory().getParentDirectory();
    struct Migration { const char* file; const char* articulation; int selected; };
    for (const auto& migration : {Migration{"folk-pipa.fytonepack", "Roll_3", 10},
                                  Migration{"folk-liuqin.fytonepack", "Roll_2", 7},
                                  Migration{"folk-jinghu.fytonepack", "Tremolo", 9}}) {
        juce::String error;
        const auto before = fengyin::TonePackage::read(repository.getChildFile("tests/fixtures/kong-dual-slot-before-133").getChildFile(migration.file), error);
        const auto after = fengyin::TonePackage::read(repository.getChildFile("assets/tone-packages").getChildFile(migration.file), error);
        assert(before && after);
        const auto original = fengyin::KongProjectFile::readPluginComponentState(before->samplerState);
        const auto migrated = fengyin::KongProjectFile::readPluginComponentState(after->samplerState);
        const auto originalMain = original.getChildWithName("PresetList").getChild(0);
        const auto list = migrated.getChildWithName("PresetList");
        assert(list.getNumChildren() == 2 && list.getChild(0).isEquivalentTo(originalMain));
        assert(migrated.getChildWithName("RackParam").isEquivalentTo(original.getChildWithName("RackParam")));
        const auto technique = list.getChild(1);
        assert(int(technique["MIDI"]) == 1 && int(technique["IndexSelectedKeyswitch"]) == migration.selected);
        assert(technique["KeyswitchNames"][migration.selected].toString() == migration.articulation);
        for (const auto* field : {"KAI", "Preset", "Audio", "Volume", "Tone", "Balance", "FXAUX"})
            assert(technique[field] == originalMain[field]);
        assert(after->kongTechniques.otherChannel == 2 && after->kongTechniques.otherKeyswitchIndex == -1);
    }
    juce::AudioProcessorGraph graph;
    graph.setPlayConfigDetails(2,2,48000,64);
    const auto input=graph.addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioInputNode));
    const auto output=graph.addNode(std::make_unique<juce::AudioProcessorGraph::AudioGraphIOProcessor>(juce::AudioProcessorGraph::AudioGraphIOProcessor::audioOutputNode));
    const auto gain=graph.addNode(std::make_unique<TestEffect>(2.0f,0.0f));
    const auto offset=graph.addNode(std::make_unique<TestEffect>(1.0f,0.25f));
    auto verify=[&](std::vector<juce::AudioProcessorGraph::Node::Ptr> order,float expected)
    {
        graph.releaseResources();
        for(const auto& connection:graph.getConnections())graph.removeConnection(connection);
        assert(fengyin::connectSerialEffects(graph,input,order,output));
        graph.prepareToPlay(48000,64);
        juce::AudioBuffer<float> buffer(2,64);juce::MidiBuffer midi;
        for(int block=0;block<3;++block)
        {
            for(int c=0;c<2;++c)for(int i=0;i<64;++i)buffer.setSample(c,i,0.1f);
            graph.processBlock(buffer,midi);
        }
        assert(std::abs(buffer.getSample(0,32)-expected)<0.0001f);
        assert(std::abs(buffer.getSample(1,32)-expected)<0.0001f);
    };
    verify({gain,offset},0.45f);
    verify({offset,gain},0.70f);
    gain->setBypassed(true);verify({gain,offset},0.35f);
    verify({},0.1f);
    // Follow device restart ordering with the same graph/nodes, not new
    // processors. Every node must receive the new period and keep its order.
    {
        for(const auto& connection:graph.getConnections())graph.removeConnection(connection);
        gain->setBypassed(false);
        assert(fengyin::connectSerialEffects(graph,input,{gain,offset},output));
        for(int cycle=0;cycle<50;++cycle)for(int size:{128,256,512,128}) {
            graph.releaseResources();
            graph.setPlayConfigDetails(2,2,48000,size);
            graph.prepareToPlay(48000,size);
            juce::AudioBuffer<float> block(2,size);juce::MidiBuffer midi;
            for(int iteration=0;iteration<3;++iteration) {
                for(int c=0;c<2;++c)for(int i=0;i<size;++i)block.setSample(c,i,.1f);
                graph.processBlock(block,midi);
            }
            for(auto node:{gain,offset}) {
                auto* effect=static_cast<TestEffect*>(node->getProcessor());
                assert(effect->preparedBlockSize==size && effect->preparedRate==48000);
            }
            for(int c=0;c<2;++c)for(int i=0;i<size;++i) {
                if(std::abs(block.getSample(c,i)-.45f)>=1e-6f)
                    std::cerr<<"restart cycle="<<cycle<<" size="<<size<<" sample="<<i<<" value="<<block.getSample(c,i)<<std::endl;
                assert(std::abs(block.getSample(c,i)-.45f)<1e-6f);
            }
        }
    }
    auto* instrument = static_cast<TestEffect*>(offset->getProcessor());
    // Exercise JUCE's actual node callback lock. The injected lock stall must
    // be attributed to waiting, not to the fake plug-in's trivial DSP body.
    {
        using namespace fengyin::audioTrace;
        struct Context {
            Accumulator timing;
            std::atomic<bool> waiting{false},done{false};
        } context;
        context.timing.instrument.store(instrument);
        Sink sink{&context,[](void* p,Event event,const void* owner,uint64_t ns){
            static_cast<Context*>(p)->timing.record(event,owner,ns);
        },[](void* p,Event event,const void* owner){
            auto* ctx=static_cast<Context*>(p);
            if(event==Event::nodeLock&&owner==ctx->timing.instrument.load())ctx->waiting.store(true);
        }};
        juce::AudioBuffer<float> block(2,128);block.clear();juce::MidiBuffer messages;
        instrument->getCallbackLock().enter();
        std::thread worker([&]{Session trace(&sink);graph.processBlock(block,messages);context.done.store(true);});
        for(int n=0;n<2000&&!context.waiting.load();++n)juce::Thread::sleep(1);
        assert(context.waiting.load()&&!context.done.load());
        juce::Thread::sleep(30);
        assert(!context.done.load());
        instrument->getCallbackLock().exit();worker.join();
        const auto metrics=context.timing.take();
        const auto& wait=metrics[static_cast<unsigned>(Event::nodeLock)*3+1];
        const auto& body=metrics[static_cast<unsigned>(Event::nodeBody)*3+1];
        assert(wait.calls==1&&wait.totalNs>=20000000);
        assert(body.calls==1&&body.totalNs<wait.totalNs);
        for(int c=0;c<2;++c)for(int i=0;i<128;++i)
            assert(std::abs(block.getSample(c,i)-.25f)<1e-6f);
        assert(current==nullptr);
    }
    const auto preparations = instrument->prepareCount, releases = instrument->releaseCount;
    {
        const fengyin::ScopedGraphPause pause(&graph);
        juce::MemoryBlock snapshot;
        instrument->getStateInformation(snapshot);
        assert(graph.isSuspended());
        {
            const fengyin::ScopedGraphPause nested(&graph);
            assert(graph.isSuspended());
        }
        assert(graph.isSuspended());
        for(const auto& connection:graph.getConnections())graph.removeConnection(connection);
        assert(fengyin::connectSerialEffects(graph,input,{offset},output));
    }
    assert(!graph.isSuspended());
    juce::AudioBuffer<float> liveBuffer(2,64);liveBuffer.clear();juce::MidiBuffer liveMidi;
    graph.processBlock(liveBuffer,liveMidi);
    assert(instrument->prepareCount==preparations && instrument->releaseCount==releases);
    graph.releaseResources();

    const auto directory=juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("fengyin-studio-test",{},true);
    assert(directory.createDirectory());
    {
    fengyin::SoundPreset preset;
    preset.id="test";preset.name="Style";preset.instrumentChineseName="Sax";preset.pluginIdentifier="instrument";
    preset.studioDraft=true;preset.instrumentState=juce::MemoryBlock("state",5);
    preset.published=true;
    const auto publicationXml=fengyin::SoundPresetStore::toXml({preset});
    const auto publicationReload=fengyin::SoundPresetStore::fromXml(*publicationXml);
    assert(publicationReload.size()==1 && publicationReload[0].published);
    preset.instrumentModelName="Model A";
    juce::PluginDescription description;description.name="Test FX";description.version="1.2";description.pluginFormatName="VST3";description.uniqueId=42;
    preset.instrumentDescriptionXml=description.createXml()->toString();
    preset.effects.push_back({description.createXml()->toString(),juce::MemoryBlock("FX bytes",8),true});
    const auto file=directory.getChildFile("scheme.fytonepack");
    assert(fengyin::TonePackage::write(preset,file));
    juce::String importError;
    const auto imported = fengyin::TonePackage::read(file, importError);
    assert(imported && importError.isEmpty() && imported->studioDraft && imported->id != preset.id);
    assert(!imported->published); // Importing never publishes automatically.
    assert(imported->instrumentState == preset.instrumentState);
    assert(imported->instrumentModelName == preset.instrumentModelName);
    assert(imported->effects[0].state == preset.effects[0].state && imported->effects[0].bypassed);
    const auto secondImport = fengyin::TonePackage::read(file, importError);
    assert(secondImport && secondImport->id != imported->id);
    juce::ZipFile zip(file);
    assert(zip.getNumEntries()==2);
    const auto read=[&](const char* name){std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(zip.getIndexOfFileName(name)));assert(stream);return stream->readEntireStreamAsString();};
    const auto xml=read("tone.xml");
    const auto restored=fengyin::SoundPresetStore::fromXml(*juce::parseXML(xml));
    assert(restored.size()==1&&!restored[0].studioDraft);
    assert(restored[0].instrumentState==preset.instrumentState);
    assert(restored[0].effects[0].state==preset.effects[0].state&&restored[0].effects[0].bypassed);
    const auto manifest=juce::JSON::parse(read("manifest.json"));
    assert(!static_cast<bool>(manifest["pluginBinariesIncluded"]));
    assert(manifest["dependencies"].size()==2);
    assert(manifest["files"][0]["sha256"].toString()==juce::SHA256(xml.toRawUTF8(),xml.getNumBytesAsUTF8()).toHexString());
    preset.pluginBrand="kong";preset.containerInstrument=true;
    assert(!fengyin::TonePackage::write(preset,directory.getChildFile("incomplete.fytonepack")));
    preset.samplerState=juce::MemoryBlock("untouched qin state",19);
    preset.containerProjectState=juce::MemoryBlock("KAM bytes",9);
    const auto kongFile=directory.getChildFile("kong.fytonepack");
    assert(fengyin::TonePackage::write(preset,kongFile));
    const auto importedKong = fengyin::TonePackage::read(kongFile, importError);
    assert(importedKong && importedKong->containerProjectState == preset.containerProjectState
        && importedKong->samplerState == preset.samplerState);
    juce::ZipFile kongZip(kongFile);
    assert(kongZip.getNumEntries()==3);
    std::unique_ptr<juce::InputStream> kam(kongZip.createStreamForEntry(kongZip.getIndexOfFileName("instrument.KAM")));
    assert(kam&&kam->readEntireStreamAsString()=="KAM bytes");
    preset.kongTechniques={true,1,2,0};
    const auto routedFile=directory.getChildFile("kong-routed.fytonepack");
    assert(fengyin::TonePackage::write(preset,routedFile));
    const auto routed=fengyin::TonePackage::read(routedFile,importError);
    assert(routed && routed->kongTechniques.enabled && routed->kongTechniques.vibratoCC==1
        && routed->kongTechniques.otherChannel==2 && routed->kongTechniques.otherKeyswitchIndex==0);
    juce::ZipFile routedZip(routedFile);
    std::unique_ptr<juce::InputStream> routedManifest(routedZip.createStreamForEntry(routedZip.getIndexOfFileName("manifest.json")));
    assert(int(juce::JSON::parse(routedManifest->readEntireStreamAsString())["version"])==2);
    preset.kongTechniques={};
    preset.pluginBrand="kontakt";preset.containerAdapter="kontakt";
    preset.containerProjectState.reset();preset.samplerState=juce::MemoryBlock("native kontakt state",20);
    const auto kontaktFile=directory.getChildFile("kontakt.fytonepack");
    assert(fengyin::TonePackage::write(preset,kontaktFile));
    const auto importedKontakt=fengyin::TonePackage::read(kontaktFile,importError);
    assert(importedKontakt && importedKontakt->samplerState==preset.samplerState && importedKontakt->containerProjectState.getSize()==0);
    assert(juce::ZipFile(kontaktFile).getNumEntries()==2);
    // Corrupted contents with an intact old manifest are refused.
    const auto badFile=directory.getChildFile("bad.fytonepack");
    const auto makeBad=[&](const juce::String& path,const juce::String& content)
    {
        juce::ZipFile::Builder builder;
        const auto json=read("manifest.json");
        builder.addEntry(std::make_unique<juce::MemoryInputStream>(json.toRawUTF8(),json.getNumBytesAsUTF8(),true),6,"manifest.json",{});
        builder.addEntry(std::make_unique<juce::MemoryInputStream>(content.toRawUTF8(),content.getNumBytesAsUTF8(),true),6,path,{});
        assert(badFile.deleteFile());
        auto stream=badFile.createOutputStream();assert(stream&&builder.writeToStream(*stream,nullptr));
    };
    makeBad("tone.xml",xml+"damaged");
    assert(!fengyin::TonePackage::read(badFile,importError));
    makeBad("../tone.xml",xml);
    assert(!fengyin::TonePackage::read(badFile,importError));
    makeBad("manifest.json",xml);
    assert(!fengyin::TonePackage::read(badFile,importError));
    }
    const auto bundled=juce::File(juce::String::fromUTF8(__FILE__)).getParentDirectory().getParentDirectory().getChildFile("assets/tone-packages");
    const auto files=bundled.findChildFiles(juce::File::findFiles,false,"*.fytonepack");
    // 33 published packages plus the retained, non-embedded legacy violin archive.
    assert(files.size()==34);
    int routedFolkPackages=0, boostedPackages=0;
    for(const auto& file:files){
        juce::String error;const auto imported=fengyin::TonePackage::read(file,error);
        if(!imported){std::cerr<<file.getFileName()<<": "<<error<<std::endl;return 1;}
        assert(error.isEmpty());
        if (imported->outputGain > 1.5f)
        {
            ++boostedPackages;
            const float expectedDb=imported->pluginBrand=="swam"?5.f:6.f;
            assert(std::abs(imported->outputGain - std::pow(10.f,expectedDb/20.f)) < 1e-6f);
            const auto copy=directory.getChildFile("gain-roundtrip-"+file.getFileName());
            assert(fengyin::TonePackage::write(*imported,copy));
            const auto restored=fengyin::TonePackage::read(copy,error);assert(restored);
            assert(restored->outputGain==imported->outputGain);
            assert(restored->instrumentState==imported->instrumentState);
            assert(restored->samplerState==imported->samplerState);
            assert(restored->containerProjectState==imported->containerProjectState);
        }
        if(file.getFileName().startsWith("folk-")) {
            ++routedFolkPackages;
            assert(imported->pluginBrand=="kong" && imported->kongTechniques.enabled);
            assert(imported->samplerState.getSize()>0 && imported->containerProjectState.getSize()>0);
        }
    }
    assert(routedFolkPackages==13);
    assert(boostedPackages==32);
    assert(directory.deleteRecursively());
}
