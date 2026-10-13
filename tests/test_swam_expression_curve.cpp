#include "SwamExpressionCurve.h"
#include "TechniqueMidiDeduplication.h"
#include "SwamTechniqueRouting.h"
#include "SimpleTechniquePolicy.h"
#include "SwamPluginClassifier.h"

#include <cassert>
#include <cmath>
#include <iostream>
#if JUCE_MAC
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace
{
bool near(double left, double right)
{
    return std::abs(left - right) < 0.000001;
}
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    const auto source = juce::String(R"xml(<?xml version="1.0"?>
<swam><program><midimapping><MIDIRemappingTable>
  <MIDIRemappingEntry enabled="1" parameterId="expression" channel="17" messageType="1" msb="2" lsb="-1">
    <MIDIRemappingCurve input_min="4" input_max="120" out_min="9" out_max="127" shape="-0.4" symmetry="0.7" bypass="1" bipolar="1"/>
  </MIDIRemappingEntry>
  <MIDIRemappingEntry enabled="1" parameterId="growl" channel="17" messageType="1" msb="74" lsb="-1">
    <MIDIRemappingCurve input_min="12" input_max="127" out_min="0" out_max="80" shape="0.3" symmetry="0.2" bypass="0" bipolar="0"/>
  </MIDIRemappingEntry>
</MIDIRemappingTable></midimapping></program></swam>)xml");
    auto xml = juce::parseXML(source);
    assert(xml != nullptr);

    const auto result = fengyin::SwamExpressionCurve::applyToXml(*xml);
    assert(result.found);
    assert(!result.changed);
    assert(result.controller == 2);

    auto* table = xml->getChildByName("program")->getChildByName("midimapping")
        ->getChildByName("MIDIRemappingTable");
    auto* expression = table->getFirstChildElement();
    auto* curve = expression->getChildByName("MIDIRemappingCurve");
    assert(expression->getIntAttribute("msb") == 2);
    assert(expression->getIntAttribute("channel") == 17);
    assert(near(curve->getDoubleAttribute("input_min"), 4));
    assert(near(curve->getDoubleAttribute("input_max"), 120));
    assert(near(curve->getDoubleAttribute("out_min"), 9));
    assert(near(curve->getDoubleAttribute("out_max"), 127));
    assert(near(curve->getDoubleAttribute("shape"), -.4));
    assert(near(curve->getDoubleAttribute("symmetry"), .7));
    assert(curve->getIntAttribute("bypass") == 1);
    assert(curve->getIntAttribute("bipolar") == 1);

    auto* growl = expression->getNextElement();
    auto* growlCurve = growl->getChildByName("MIDIRemappingCurve");
    assert(growl->getIntAttribute("msb") == 74);
    assert(near(growlCurve->getDoubleAttribute("input_min"), 12.0));
    assert(near(growlCurve->getDoubleAttribute("out_max"), 80.0));

    juce::MemoryBlock plain(source.toRawUTF8(), static_cast<size_t>(source.getNumBytesAsUTF8()));
    const auto plainResult = fengyin::SwamExpressionCurve::applyToState(plain);
    assert(plainResult.found && !plainResult.changed && plainResult.controller == 2);
    auto rewritten = juce::parseXML(juce::String::fromUTF8(static_cast<const char*>(plain.getData()),
                                                           static_cast<int>(plain.getSize())));
    assert(rewritten != nullptr);

    juce::MemoryBlock binary;
    juce::AudioProcessor::copyXmlToBinary(*xml, binary);
    const auto binaryResult = fengyin::SwamExpressionCurve::applyToState(binary);
    assert(binaryResult.found);
    assert(! binaryResult.changed);
    assert(binaryResult.controller == 2);
    assert(binaryResult.routes.size() == 2);
    assert(binaryResult.routes[1].parameter == "growl" && binaryResult.routes[1].controller == 74);
    // Installed macOS SWAM serialises unused 7-bit LSB as 0, Windows as -1.
    expression->setAttribute("lsb", 0);
    assert(fengyin::SwamExpressionCurve::applyToXml(*xml).controller == 2);

    // Missing curves get a linear default, existing curves remain byte-for-byte.
    expression->removeChildElement(curve, true);
    const auto missing = fengyin::SwamExpressionCurve::applyToXml(*xml);
    assert(missing.changed);
    curve = expression->getChildByName("MIDIRemappingCurve");
    assert(curve && curve->getIntAttribute("input_min") == 0 && curve->getIntAttribute("out_max") == 127);
    assert(curve->getDoubleAttribute("shape") == 0);

    // Exercise both real bundled VST3 envelopes, not only simplified test XML.
    const auto root = juce::File(juce::String::fromUTF8(__FILE__)).getParentDirectory().getParentDirectory();
    for (const auto* filename : {"soprano-kenny.fytonepack", "tenor-air-pocket.fytonepack"})
    {
        juce::ZipFile zip(root.getChildFile("assets/tone-packages").getChildFile(filename));
        const int index = zip.getIndexOfFileName("tone.xml");
        assert(index >= 0);
        std::unique_ptr<juce::InputStream> stream(zip.createStreamForEntry(index));
        assert(stream);
        const auto presetXml = juce::parseXML(stream->readEntireStreamAsString());
        assert(presetXml);
        const auto* saved = presetXml->getChildByName("PRESET")->getChildByName("INSTRUMENT_STATE");
        assert(saved);
        juce::MemoryBlock savedState;
        assert(savedState.fromBase64Encoding(saved->getAllSubText()));
        const auto before = savedState;
        const auto native = fengyin::SwamExpressionCurve::applyToState(savedState);
        assert(native.found && native.controller == 11 && !native.changed);
        assert(before == savedState);
        assert(!native.routes.empty());
        auto prepared=before;
        const bool growl=juce::String(filename).startsWith("tenor");
        const auto routes=fengyin::SwamTechniqueRouting::prepare(prepared,true,growl);
        if(!routes.supported)std::cerr<<"Routing stage="<<routes.stage<<" asset="<<filename<<std::endl;
        assert(routes.supported && routes.vibrato>=0 && (!growl || routes.growl>=0));
        const auto changed=fengyin::SwamExpressionCurve::applyToState(prepared);
        assert(changed.found && changed.controller==native.controller);
        auto twice=prepared;
        assert(!fengyin::SwamTechniqueRouting::prepare(twice,true,growl).changed);
        assert(twice==prepared);
        auto flutterState=before;
        const auto flutterRouting=fengyin::SwamTechniqueRouting::prepare(flutterState,false,false,true);
        assert(flutterRouting.supported && flutterRouting.flutter>=1);
        const auto flutterOnce=flutterState;
        assert(!fengyin::SwamTechniqueRouting::prepare(flutterState,false,false,true).changed);
        assert(flutterOnce==flutterState);
        const auto decode=[](const juce::MemoryBlock& s){
            auto outer=juce::AudioProcessor::getXmlFromBinary(s.getData(),static_cast<int>(s.getSize()));juce::MemoryBlock b;
            b.fromBase64Encoding(outer->getChildByName("IComponent")->getAllSubText());
            return juce::AudioProcessor::getXmlFromBinary(static_cast<const char*>(b.getData())+176,static_cast<int>(b.getSize()-176));
        };
        auto oldXml=decode(before),newXml=decode(prepared);
        auto* oldTable=fengyin::SwamTechniqueRouting::findTable(*oldXml);
        auto* newTable=fengyin::SwamTechniqueRouting::findTable(*newXml);
        for(auto* oldEntry:oldTable->getChildIterator()){
            auto* oldCurve=oldEntry->getChildByName("MIDIRemappingCurve");if(!oldCurve)continue;
            juce::XmlElement* newEntry=nullptr;
            for(auto* e:newTable->getChildIterator())if(e->getStringAttribute("parameterId")==oldEntry->getStringAttribute("parameterId"))newEntry=e;
            assert(newEntry && oldCurve->isEquivalentTo(newEntry->getChildByName("MIDIRemappingCurve"), false));
        }
        oldXml->findParentElementOf(oldTable)->removeChildElement(oldTable,true);
        newXml->findParentElementOf(newTable)->removeChildElement(newTable,true);
        assert(oldXml->isEquivalentTo(newXml.get(), false)); // All non-mapping state is preserved.
    }
    // Optional installed-plugin verification: no audio device, no saved settings.
    if (argc >= 2)
    {
        if (juce::String(argv[1]) == "--inspect-json" && argc >= 3)
        {
            const auto diagnostic = juce::JSON::parse(juce::File(juce::String::fromUTF8(argv[2])));
            juce::MemoryOutputStream decoded;
            if (!juce::Base64::convertFromBase64(decoded, diagnostic["stateBase64"].toString())) return 7;
            auto state = decoded.getMemoryBlock();
            const auto result = fengyin::SwamExpressionCurve::applyToState(state);
            std::cout << "Expression found=" << result.found << " changed=" << result.changed << '\n';
            for (const auto& route : result.routes)
                std::cout << route.parameter << " channel=" << route.channel << " CC=" << route.controller << '\n';
            return result.found ? 0 : 8;
        }
        juce::AudioPluginFormatManager formats;
        formats.addFormat(std::make_unique<juce::VST3PluginFormat>());
        juce::OwnedArray<juce::PluginDescription> descriptions;
        formats.getFormat(0)->findAllTypesForFile(descriptions, juce::String::fromUTF8(argv[1]));
        if (descriptions.isEmpty()) return 2;
        std::cout << "PROBE plugin=" << descriptions[0]->name << " version=" << descriptions[0]->version << std::endl;
        if (argc >= 4)
        {
            const auto diagnostic = juce::JSON::parse(juce::File(juce::String::fromUTF8(argv[3])));
            if (diagnostic["pluginName"].toString() != descriptions[0]->name
                || diagnostic["pluginVersion"].toString() != descriptions[0]->version)
            {
                std::cerr << "Exact-state reproduction unavailable: diagnostic plugin/version differs from installed plugin\n";
                return 9;
            }
        }
        juce::String error;
        auto plugin = formats.createPluginInstance(*descriptions[0], 48000, 256, error);
        if (!plugin) { std::cerr << error << '\n'; return 3; }
        plugin->setNonRealtime(true);
        if (argc >= 4)
        {
            const auto diagnostic = juce::JSON::parse(juce::File(juce::String::fromUTF8(argv[3])));
            juce::MemoryOutputStream decoded;
            if (!juce::Base64::convertFromBase64(decoded, diagnostic["stateBase64"].toString())) return 7;
            const auto started = juce::Time::getMillisecondCounterHiRes();
            plugin->setStateInformation(decoded.getData(), static_cast<int>(decoded.getDataSize()));
            std::cout << "PERF imported-state-ms=" << juce::Time::getMillisecondCounterHiRes()-started << '\n';
        }
        plugin->prepareToPlay(48000, 256);
#if JUCE_MAC
        // Optional real-plugin probe must service asynchronous native startup.
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 1.0, false);
#endif
        juce::MemoryBlock state;
        plugin->getStateInformation(state);
        const auto before = state;
        if(argc>=3 && juce::String(argv[2])=="--routing")
        {
            std::vector<float> values;for(auto* p:plugin->getParameters())values.push_back(p->getValue());
            auto routed=state;
            const bool needsGrowl=fengyin::simpleGrowl(fengyin::SwamPluginClassifier::instrumentKey(plugin->getName().toStdString()));
            const auto plan=fengyin::SwamTechniqueRouting::prepare(routed,true,needsGrowl);
            if(!plan.supported)return 20;
            if(plan.changed)plugin->setStateInformation(routed.getData(),static_cast<int>(routed.getSize()));
            juce::MemoryBlock restored;plugin->getStateInformation(restored);
            auto inspect=restored;const auto checked=fengyin::SwamTechniqueRouting::prepare(inspect,true,needsGrowl);
            if(!checked.supported||checked.changed||checked.vibrato!=plan.vibrato||checked.growl!=plan.growl)return 21;
            int index=0;for(auto* p:plugin->getParameters())if(std::abs(p->getValue()-values[static_cast<size_t>(index++)])>.00001f)return 22;
            auto routes=fengyin::SwamExpressionCurve::applyToState(restored);
            {
                auto o=juce::AudioProcessor::getXmlFromBinary(restored.getData(),static_cast<int>(restored.getSize()));juce::MemoryBlock b;
                b.fromBase64Encoding(o->getChildByName("IComponent")->getAllSubText());
                auto x=juce::AudioProcessor::getXmlFromBinary(static_cast<const char*>(b.getData())+176,static_cast<int>(b.getSize()-176));
                if(auto* t=fengyin::SwamTechniqueRouting::findTable(*x))for(auto* e:t->getChildIterator())if(e->getStringAttribute("parameterId").equalsIgnoreCase("vibratoDepth"))std::cout<<e->toString()<<std::endl;
            }
            int vibratoChannel=1,growlChannel=1;
            for(const auto& r:routes.routes){if(r.parameter.equalsIgnoreCase("vibratoDepth"))vibratoChannel=r.channel;if(r.parameter.equalsIgnoreCase("growl"))growlChannel=r.channel;}
            juce::AudioBuffer<float> audio(2,256);juce::MidiBuffer midi;
            midi.addEvent(juce::MidiMessage::controllerEvent(1,routes.controller,80),0);
            midi.addEvent(juce::MidiMessage::noteOn(1,60,static_cast<juce::uint8>(80)),0);
            plugin->processBlock(audio,midi);midi.clear();
            midi.addEvent(juce::MidiMessage::controllerEvent(vibratoChannel,plan.vibrato,127),0);
            if(needsGrowl)midi.addEvent(juce::MidiMessage::controllerEvent(growlChannel,plan.growl,127),0);
            for(int block=0;block<128;++block){
                audio.clear();midi.clear();
                midi.addEvent(juce::MidiMessage::controllerEvent(vibratoChannel,plan.vibrato,127),0);
                if(needsGrowl)midi.addEvent(juce::MidiMessage::controllerEvent(growlChannel,plan.growl,127),0);
                plugin->processBlock(audio,midi);
            }
#if JUCE_MAC
            CFRunLoopRunInMode(kCFRunLoopDefaultMode,0.1,false);
#endif
            bool vibrato=false,growl=false;for(auto* p:plugin->getParameters()){
                if(p->getName(80)=="Vibrato Depth"){vibrato=p->getValue()>0;std::cout<<"Vibrato Depth="<<p->getValue()<<"\n";}
                if(p->getName(80)=="Growl"){growl=p->getValue()>0;std::cout<<"Growl="<<p->getValue()<<"\n";}
            }
            // VST3 parameter getters report the saved base values, not always
            // the live MIDI modulation. Verify the rendered signal instead.
            const auto render=[&](int v,int g){
                std::vector<float> samples;
                for(int block=0;block<600;++block){
                    audio.clear();midi.clear();
                    if(block==0){midi.addEvent(juce::MidiMessage::controllerEvent(vibratoChannel,plan.vibrato,v),0);if(needsGrowl)midi.addEvent(juce::MidiMessage::controllerEvent(growlChannel,plan.growl,g),0);}
                    plugin->processBlock(audio,midi);
                    if(block>=200)samples.insert(samples.end(),audio.getReadPointer(0),audio.getReadPointer(0)+256);
                }
                return samples;
            };
            const auto baseline=render(0,0);
            int centreLag=0;double bestPeriod=-1;
            for(int lag=80;lag<=700;++lag){double sum=0,e1=0,e2=0;for(int i=0;i<4096;++i){sum+=baseline[static_cast<size_t>(i)]*baseline[static_cast<size_t>(i+lag)];e1+=baseline[static_cast<size_t>(i)]*baseline[static_cast<size_t>(i)];e2+=baseline[static_cast<size_t>(i+lag)]*baseline[static_cast<size_t>(i+lag)];}const auto c=sum/std::sqrt(e1*e2);if(c>bestPeriod){bestPeriod=c;centreLag=lag;}}
            const auto analyse=[centreLag](const std::vector<float>& s){
                std::vector<double> pitches;double power=0,correlation=0;int windows=0;
                for(size_t start=0;start+4096<s.size();start+=4096){
                    double best=-1;int bestLag=0;double energy=0;
                    for(int i=0;i<2048;++i)energy+=s[start+i]*s[start+i];
                    power+=energy;
                    if(energy<1e-12)continue;
                    for(int lag=static_cast<int>(centreLag*.85);lag<=static_cast<int>(centreLag*1.15);++lag){double sum=0,e2=0;for(int i=0;i<2048;++i){sum+=s[start+i]*s[start+i+lag];e2+=s[start+i+lag]*s[start+i+lag];}const auto c=sum/std::sqrt(energy*e2);if(c>best){best=c;bestLag=lag;}}
                    pitches.push_back(1200*std::log2(48000.0/bestLag/261.625565));correlation+=best;++windows;
                }
                double low=1e9,high=-1e9;for(auto p:pitches){low=std::min(low,p);high=std::max(high,p);}
                return std::array<double,3>{windows?high-low:0,windows?correlation/windows:0,power};
            };
            const auto off=analyse(baseline);const auto on=analyse(render(127,0));const auto growled=analyse(render(0,127));
            std::cout<<"Rendered pitch spread off="<<off[0]<<" on="<<on[0]<<" correlation off="<<off[1]<<" growl="<<growled[1]<<" power="<<off[2]<<"\n";
            if(off[2]<=1e-9 || on[0]<=off[0]+2.0)return 23;
            if(needsGrowl && std::abs(growled[1]-off[1])<.005)return 24;
            plugin->releaseResources();std::cout<<"PASS native routing readback, preserved parameters, CC drives both techniques\n";return 0;
        }
        const auto mapping = fengyin::SwamExpressionCurve::applyToState(state);
        if (!mapping.found || state != before)
        {
            std::cerr << "Native mapping unavailable: stateBytes=" << state.getSize() << " routes=" << mapping.routes.size() << '\n';
            if (auto outer = juce::AudioProcessor::getXmlFromBinary(state.getData(), (int)state.getSize()))
            {
                std::cerr << "Outer state=" << outer->getTagName() << '\n';
                if (auto* component = outer->getChildByName("IComponent"))
                {
                    juce::MemoryBlock payload;
                    if (payload.fromBase64Encoding(component->getAllSubText()))
                    {
                        const std::string bytes((const char*)payload.getData(), payload.getSize());
                        const auto position = bytes.find("<MIDIRemappingTable");
                        std::cerr << "Component bytes=" << bytes.size() << " table=" << position << '\n';
                        if (position != std::string::npos) std::cerr << bytes.substr(position, 900) << '\n';
                    }
                }
            }
            return 4;
        }
        juce::AudioBuffer<float> audio(juce::jmax(2, plugin->getTotalNumOutputChannels()), 256);
        double largest = 0;
        for (int cc : {0, 16, 32, 64, 96, 127, 0})
        {
            double energy = 0;
            for (int block = 0; block < 100; ++block)
            {
                audio.clear(); juce::MidiBuffer midi;
                if (block == 0)
                {
                    midi.addEvent(juce::MidiMessage::controllerEvent(1, mapping.controller, cc), 0);
                    if (cc == 16) midi.addEvent(juce::MidiMessage::noteOn(1, 60, (juce::uint8)80), 1);
                    if (cc == 0) midi.addEvent(juce::MidiMessage::noteOff(1, 60), 1);
                }
                plugin->processBlock(audio, midi);
                for (int c = 0; c < audio.getNumChannels(); ++c)
                    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
                    {
                        const auto v = audio.getSample(c, sample);
                        if (!std::isfinite(v)) return 5;
                        energy += double(v) * v;
                    }
            }
            largest = std::max(largest, energy);
            std::cout << "native CC=" << cc << " energy=" << energy << '\n';
        }
        if (argc >= 3 && juce::String(argv[2]) == "--benchmark")
        {
            const auto now = [] { return juce::Time::getMillisecondCounterHiRes(); };
            const auto start = now();
            for (int n = 0; n < 50; ++n)
            {
                juce::MemoryBlock snapshot;
                plugin->getStateInformation(snapshot);
                fengyin::SwamExpressionCurve::applyToState(snapshot);
            }
            std::cout << "PERF state-read-and-parse-ms=" << (now()-start)/50 << '\n';
            std::vector<fengyin::SwamExpressionCurve::Result::Route> techniques;
            for (const auto& route : mapping.routes)
            {
                std::cout << "ROUTE " << route.parameter << " cc=" << route.controller << '\n';
                if (route.parameter == "growl" || route.parameter == "vibratoDepth"
                    || route.parameter == "portamento") techniques.push_back(route);
            }
            juce::AudioProcessorParameter* legacyVibrato = nullptr;
            for (auto* parameter : plugin->getParameters())
                if (parameter->getName(128).toLowerCase().removeCharacters(" ._-") == "vibratodepth")
                    legacyVibrato = parameter;
            for (int pass = 0; pass < 3; ++pass)
              for (int density : {-1, 0, 1, 8, 9})
              {
                double elapsed = 0;
                std::atomic<int> dedupCache {0};
                for (int block = 0; block < 2000; ++block)
                {
                    audio.clear(); juce::MidiBuffer midi;
                    midi.addEvent(juce::MidiMessage::controllerEvent(1, mapping.controller, 80), 0);
                    if (block == 0) midi.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)80),0);
                    for (int event = 0; event < (density == 9 ? 8 : density); ++event)
                        for (const auto& route : techniques)
                        {
                            auto send = [&] {
                                midi.addEvent(juce::MidiMessage::controllerEvent(route.channel, route.controller, 0), event*16);
                                return true;
                            };
                            if (density == 9)
                                fengyin::sendTechniqueController(dedupCache,(route.channel << 8) | (route.controller+1),0,true,send);
                            else send();
                        }
                    const auto begin = now();
                    if (density == -1 && legacyVibrato != nullptr)
                        legacyVibrato->setValueNotifyingHost(0.0f);
                    plugin->processBlock(audio, midi);
                    elapsed += now()-begin;
                }
                std::cout << "PERF pass=" << pass << " mode=" << (density == 9 ? "deduplicated-8" : density == -1 ? "legacy-parameter" : "raw-midi")
                          << " input-events-per-block=" << (density == 9 ? 8 : density)*static_cast<int>(techniques.size())
                          << " callback-us=" << elapsed*1000/2000 << '\n';
                juce::MidiBuffer off;
                off.addEvent(juce::MidiMessage::allNotesOff(1),0);
                plugin->processBlock(audio,off);
              }
            for (const auto& name : {"growl", "flutter", "vibratodepth"})
            {
                juce::AudioProcessorParameter* target = nullptr;
                for (auto* parameter : plugin->getParameters())
                    if (parameter->getName(128).toLowerCase().removeCharacters(" ._-") == name)
                        target = parameter;
                if (!target) continue;
                for (float value : {0.0f, 0.85f, 0.0f})
                {
                    target->setValueNotifyingHost(value);
                    double elapsed = 0;
                    for (int block = 0; block < 1000; ++block)
                    {
                        audio.clear(); juce::MidiBuffer midi;
                        if (block == 0)
                        {
                            midi.addEvent(juce::MidiMessage::controllerEvent(1, mapping.controller, 100), 0);
                            midi.addEvent(juce::MidiMessage::noteOn(1,60,(juce::uint8)80), 0);
                        }
                        const auto begin = now();
                        plugin->processBlock(audio,midi);
                        elapsed += now()-begin;
                    }
                    std::cout << "PERF technique=" << name << " value=" << value << " callback-us=" << elapsed << '\n';
                    juce::MidiBuffer off;
                    off.addEvent(juce::MidiMessage::allNotesOff(1),0);
                    plugin->processBlock(audio,off);
                }
            }
        }
        plugin->releaseResources();
        if (largest <= 1e-9) return 6;
        std::cout << "Real plugin MIDI render passed; saved curves unchanged\n";
    }
    return 0;
}
