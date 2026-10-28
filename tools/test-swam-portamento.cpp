// Optional installed-plugin integration test. Runs offline; no audio device or user preset writes.
#include "PluginHostEngine.h"
#include "SwamStateCompatibility.h"
#include "SwamTechniqueParameterPolicy.h"
#include "TonePackage.h"
#include <CoreFoundation/CoreFoundation.h>
#include <cassert>
#include <iostream>
namespace fengyin { struct PluginHostEngineTestAccess {
 static juce::AudioProcessor* effect(PluginHostEngine& h){return h.effectNodes.front().node->getProcessor();}
};}
static void pump(double seconds){const auto end=juce::Time::getMillisecondCounterHiRes()+seconds*1000;while(juce::Time::getMillisecondCounterHiRes()<end)CFRunLoopRunInMode(kCFRunLoopDefaultMode,.01,true);}
static void await(bool& done){for(int i=0;i<3000&&!done;++i)pump(.01);assert(done);}
static void compare(const juce::MemoryBlock& before,const juce::MemoryBlock& after){
 if(fengyin::SwamStateCompatibility::preserved(before,after))return;
 auto a=fengyin::SwamStateCompatibility::read(before),b=fengyin::SwamStateCompatibility::read(after);assert(a&&b);
 std::map<juce::String,double> x,y;fengyin::SwamStateCompatibility::params(*a,x);fengyin::SwamStateCompatibility::params(*b,y);
 for(const auto& [k,v]:x)if(!y.count(k)||std::abs(v-y[k])>.0001)std::cerr<<"CHANGED "<<k<<": "<<v<<" -> "<<y[k]<<std::endl;
 if(!fengyin::SwamStateCompatibility::table(*a)->isEquivalentTo(fengyin::SwamStateCompatibility::table(*b),true))std::cerr<<"MIDI mapping changed"<<std::endl;
 assert(false);
}
// Ambiente timestamps each getStateInformation call. Exclude only that clock
// metadata; compare every other XML field and the complete wrapper header.
static bool sameAmbienteState(const juce::MemoryBlock& a, const juce::MemoryBlock& b)
{
 if (a == b) return true;
 auto x = juce::AudioProcessor::getXmlFromBinary(a.getData(), int(a.getSize()));
 auto y = juce::AudioProcessor::getXmlFromBinary(b.getData(), int(b.getSize()));
 if (!x || !y) return false;
 auto* xc = x->getChildByName("IComponent");
 auto* yc = y->getChildByName("IComponent");
 if (!xc || !yc) return false;
 juce::MemoryBlock xb, yb;
 if (!xb.fromBase64Encoding(xc->getAllSubText()) || !yb.fromBase64Encoding(yc->getAllSubText())) return false;
 constexpr size_t offset = 176; // Verified Ambiente VstW/CcnK wrapper.
 if (xb.getSize() != yb.getSize() || xb.getSize() <= offset
     || std::memcmp(xb.getData(), yb.getData(), offset) != 0) return false;
 auto xi = juce::AudioProcessor::getXmlFromBinary(static_cast<const char*>(xb.getData()) + offset, int(xb.getSize() - offset));
 auto yi = juce::AudioProcessor::getXmlFromBinary(static_cast<const char*>(yb.getData()) + offset, int(yb.getSize() - offset));
 if (!xi || !yi || xi->getStringAttribute("type") != "Ambiente" || yi->getStringAttribute("type") != "Ambiente") return false;
 xi->removeChildElement(xi->getChildByName("datetime"), true);
 yi->removeChildElement(yi->getChildByName("datetime"), true);
 xc->deleteAllChildElements(); yc->deleteAllChildElements();
 return xi->isEquivalentTo(yi.get(), true) && x->isEquivalentTo(y.get(), true);
}
int main(int argc,char** argv){
 juce::ScopedJuceInitialiser_GUI gui;juce::AudioPluginFormatManager fm;fm.addFormat(std::make_unique<juce::VST3PluginFormat>());
 const auto dir=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("fy-state-test-"+juce::Uuid().toString());assert(dir.createDirectory());
 int instruments=0,totalParams=0;
 for(const auto* name:{"SWAM Violin 3","SWAM Cello 3","SWAM Alto Sax 3","SWAM Flute 3","SWAM Trumpet"}){
  juce::OwnedArray<juce::PluginDescription> types;fm.getFormat(0)->findAllTypesForFile(types,juce::String("/Library/Audio/Plug-Ins/VST3/")+name+".vst3");assert(!types.isEmpty());
  fengyin::PluginHostEngine host;bool done=false;host.loadAsync(*types[0],48000,128,[&](bool ok,const juce::String& message){if(!ok)std::cerr<<message<<std::endl;assert(ok);done=true;});await(done);pump(.1);
  if(instruments==0&&argc==2){juce::MemoryBlock fixture;assert(juce::File(juce::String::fromUTF8(argv[1])).loadFileAsData(fixture));host.getPlugin()->setStateInformation(fixture.getData(),int(fixture.getSize()));}
  assert(host.applyExpressionAttackControl());
  int changed=0;for(auto* p:host.getPlugin()->getParameters()){
   const auto n=p->getName(128);if(n=="Portamento Time"||n=="Vibrato Rate"||n=="Reverb Mix"||n=="Bow Pressure"||n=="Formant"||n=="Breath Noise"||n=="Pan"||n=="Harmonic Structure"||n=="Tremolo Min Speed"){
    p->setValueNotifyingHost(.37f);++changed;
   }
  }assert(changed>=2);pump(.05);
  auto original=host.captureInstrumentState();auto xml=fengyin::SwamStateCompatibility::read(original);assert(xml);std::map<juce::String,double> parameters;fengyin::SwamStateCompatibility::params(*xml,parameters);totalParams+=int(parameters.size());
  if(instruments==0&&argc==2)assert(fengyin::SwamTechniqueParameterPolicy::hasNativePortamentoInput(*xml));
  assert(host.applyStandardSwamExpressionCurve());host.flushTechniqueValues();compare(original,host.captureInstrumentState());
  // An actual third-party effect state travels through the same save/load path.
  juce::OwnedArray<juce::PluginDescription> fx;fm.getFormat(0)->findAllTypesForFile(fx,"/Library/Audio/Plug-Ins/VST3/Ambiente.vst3");assert(!fx.isEmpty());done=false;host.loadEffectAsync(*fx[0],48000,128,[&](bool ok,const juce::String&){assert(ok);done=true;});await(done);
  auto* effect=fengyin::PluginHostEngineTestAccess::effect(host);for(auto* p:effect->getParameters())if(p->isAutomatable()&&!p->isBoolean()&&!p->isDiscrete())p->setValueNotifyingHost(.31f);host.setEffectBypassedAt(0,true);
  const auto effects=host.captureEffectChain();assert(effects.size()==1&&effects[0].state.getSize()>0);
  for(int cycle=0;cycle<3;++cycle){
   fengyin::SoundPreset preset;preset.id="test";preset.name=name;preset.instrumentChineseName=name;preset.pluginIdentifier=types[0]->createIdentifierString();preset.instrumentDescriptionXml=types[0]->createXml()->toString();preset.instrumentState=original;preset.effects=effects;preset.outputGain=.67f;preset.reverbMix=.17f;preset.bass=.13f;preset.air=.23f;
   fengyin::SoundPresetStore store(dir);assert(store.save(preset));auto saved=store.findById("test");assert(saved);assert(saved->instrumentState==original);
   auto pack=dir.getChildFile("test.fytonepack");assert(fengyin::TonePackage::write(*saved,pack));juce::String err;auto imported=fengyin::TonePackage::read(pack,err);assert(imported);assert(imported->instrumentState==original&&imported->effects[0].state==effects[0].state);assert(imported->outputGain==preset.outputGain&&imported->reverbMix==preset.reverbMix&&imported->bass==preset.bass&&imported->air==preset.air);
   host.unload();done=false;host.loadAsync(*types[0],48000,128,imported->instrumentState,[&](bool ok,const juce::String&){assert(ok);done=true;},true);await(done);assert(host.applyStandardSwamExpressionCurve());assert(host.applyExpressionAttackControl());host.flushTechniqueValues();compare(original,host.captureInstrumentState());
   done=false;host.restoreEffectChain(imported->effects,48000,128,[&](bool ok,const juce::String&){assert(ok);done=true;});await(done);auto restored=host.captureEffectChain();assert(restored.size()==effects.size()&&restored[0].bypassed);
   assert(sameAmbienteState(effects[0].state, restored[0].state));
  }
  std::cout<<"PASS "<<name<<": "<<parameters.size()<<" saved parameters + complete MIDI mapping table; 3 store/package/reload cycles; Ambiente state/bypass + FengYin FX retained"<<std::endl;++instruments;
 }
 std::cout<<"PASS total "<<instruments<<" real instruments, "<<totalParams<<" parameters"<<std::endl;
 dir.deleteRecursively();
}
