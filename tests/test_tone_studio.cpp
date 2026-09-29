#include "TonePackage.h"
#include "SerialEffectRouting.h"
#include "ScopedGraphPause.h"
#include <cassert>
#include <cmath>

class TestEffect final : public juce::AudioProcessor
{
public:
    TestEffect(float multiplier, float offset) : AudioProcessor(BusesProperties()
        .withInput("In", juce::AudioChannelSet::stereo()).withOutput("Out", juce::AudioChannelSet::stereo())),
        gain(multiplier), bias(offset) {}
    const juce::String getName() const override { return "TestEffect"; }
    int prepareCount = 0, releaseCount = 0;
    void prepareToPlay(double, int) override { ++prepareCount; }
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
    juce::ScopedJuceInitialiser_GUI init;
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
    auto* instrument = static_cast<TestEffect*>(offset->getProcessor());
    const auto preparations = instrument->prepareCount, releases = instrument->releaseCount;
    {
        const fengyin::ScopedGraphPause pause(&graph);
        juce::MemoryBlock snapshot;
        instrument->getStateInformation(snapshot);
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
    juce::PluginDescription description;description.name="Test FX";description.version="1.2";description.pluginFormatName="VST3";description.uniqueId=42;
    preset.instrumentDescriptionXml=description.createXml()->toString();
    preset.effects.push_back({description.createXml()->toString(),juce::MemoryBlock("FX bytes",8),true});
    const auto file=directory.getChildFile("scheme.fytonepack");
    assert(fengyin::TonePackage::write(preset,file));
    juce::String importError;
    const auto imported = fengyin::TonePackage::read(file, importError);
    assert(imported && importError.isEmpty() && imported->studioDraft && imported->id != preset.id);
    assert(imported->instrumentState == preset.instrumentState);
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
    assert(directory.deleteRecursively());
}
