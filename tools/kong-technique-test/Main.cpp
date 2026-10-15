#include <juce_audio_utils/juce_audio_utils.h>
#include "KongReleaseStates.h"
#include "KongProjectFile.h"
#include "KongPerformancePolicy.h"
#include <iostream>
#include <cmath>
#if JUCE_WINDOWS
#include <windows.h>
#include <shellapi.h>
#endif

namespace {
juce::StringArray arguments(int argc,char** argv){juce::StringArray result;
#if JUCE_WINDOWS
 int count=0;auto** words=CommandLineToArgvW(GetCommandLineW(),&count);if(!words)throw std::runtime_error("Cannot parse Windows arguments");for(int i=0;i<count;++i)result.add(juce::String(words[i]));LocalFree(words);
#else
 for(int i=0;i<argc;++i)result.add(juce::String::fromUTF8(argv[i]));
#endif
 return result;}
constexpr double rate=44100; constexpr int blockSize=128;
void pump(int ms){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}
void json(const juce::File& f,juce::DynamicObject::Ptr o){if(!f.replaceWithText(juce::JSON::toString(juce::var(o.get()),true)))throw std::runtime_error("Cannot write report");}
struct Case {juce::String id;int kind=0,key=-1,param=-1;};
class FakePlugin final : public juce::AudioPluginInstance {
public:
 int starts=0;bool active=false;double phase=0;
 const juce::String getName() const override{return "Test signal only";}
 void fillInPluginDescription(juce::PluginDescription&)const override{}
 void prepareToPlay(double,int)override{} void releaseResources()override{}
 bool acceptsMidi()const override{return true;}bool producesMidi()const override{return false;}
 double getTailLengthSeconds()const override{return 0;}bool hasEditor()const override{return false;}
 juce::AudioProcessorEditor* createEditor()override{return nullptr;}
 int getNumPrograms()override{return 1;}int getCurrentProgram()override{return 0;}void setCurrentProgram(int)override{}
 const juce::String getProgramName(int)override{return {}; }void changeProgramName(int,const juce::String&)override{}
 void getStateInformation(juce::MemoryBlock&)override{}void setStateInformation(const void*,int)override{}
 void processBlock(juce::AudioBuffer<float>& a,juce::MidiBuffer& m)override{for(auto e:m){const auto v=e.getMessage();if(v.isNoteOn()&&v.getNoteNumber()==67){++starts;active=true;}if(v.isNoteOff()&&v.getNoteNumber()==67)active=false;}for(int c=0;c<a.getNumChannels();++c)for(int i=0;i<a.getNumSamples();++i)a.setSample(c,i,active?.1f:0);}
};
struct Test {
 juce::AudioPluginFormatManager formats;juce::PluginDescription description;juce::MemoryBlock original;juce::File out;int normal=24,note=67,channel=1;
 Test(){formats.addFormat(std::make_unique<juce::VST3PluginFormat>());}
 std::unique_ptr<juce::AudioPluginInstance> load(){juce::String error;auto p=formats.createPluginInstance(description,rate,blockSize,error);if(!p)throw std::runtime_error(error.toStdString());pump(500);p->setStateInformation(original.getData(),(int)original.getSize());pump(600);p->setPlayConfigDetails(0,2,rate,blockSize);p->setNonRealtime(false);p->prepareToPlay(rate,blockSize);pump(200);return p;}
 juce::ValueTree state(juce::AudioPluginInstance& p){juce::MemoryBlock b;p.getStateInformation(b);return fengyin::KongProjectFile::readPluginComponentState(b);}
 void render(juce::AudioPluginInstance& p,const Case& c,int playedNote,double seconds,const juce::File& wav,bool probe=false){
  auto stream=wav.createOutputStream();if(!stream)throw std::runtime_error("Cannot open WAV");juce::WavAudioFormat format;std::unique_ptr<juce::AudioFormatWriter> writer(format.createWriterFor(stream.get(),rate,2,16,{},0));if(!writer)throw std::runtime_error("Cannot create WAV");stream.release();
  auto events=wav.withFileExtension(".events.tsv").createOutputStream();if(!events)throw std::runtime_error("Cannot open event log");*events<<"sample\tseconds\thex\n";
  auto metrics=wav.withFileExtension(".metrics.tsv").createOutputStream();if(!metrics)throw std::runtime_error("Cannot open metrics");*metrics<<"seconds\trms\tpeak\n";
  juce::AudioBuffer<float> a(2,blockSize);juce::MidiBuffer midi;const int total=(int)(seconds*rate);int on=(int)(.25*rate),off=(int)((seconds-.4)*rate);bool selected=false,ended=false;double power=0;float peak=0;int signal=0;const auto renderStarted=juce::Time::getMillisecondCounterHiRes();
  for(int pos=0;pos<total;pos+=blockSize){a.clear();midi.clear();auto send=[&](juce::MidiMessage m,int offset=0){midi.addEvent(m,offset);*events<<juce::String(pos+offset)<<"\t"<<juce::String((pos+offset)/rate,6)<<"\t"<<juce::String::toHexString(m.getRawData(),m.getRawDataSize())<<"\n";};
   if(pos==0){send(juce::MidiMessage::controllerEvent(channel,121,0));send(juce::MidiMessage::pitchWheel(channel,8192));send(juce::MidiMessage::controllerEvent(channel,1,0));send(juce::MidiMessage::channelPressureChange(channel,0));send(juce::MidiMessage::noteOn(channel,normal,(juce::uint8)100));send(juce::MidiMessage::noteOff(channel,normal));}
   if(pos% (blockSize*8)==0)send(fengyin::kongBreathExpression(channel,95));
   if(!selected&&pos+blockSize>on){send(juce::MidiMessage::noteOn(channel,playedNote,(juce::uint8)95),juce::jmax(0,on-pos));selected=true;}
   const auto t=pos/rate;
   if(!probe){
    const int atOn=(int)(2*rate),atOff=(int)(5*rate);
    for(int edge=0;edge<2;++edge){const int at=edge?atOff:atOn;if(pos<=at&&pos+blockSize>at){if(c.kind==1||c.kind==2){if(c.kind==2)send(juce::MidiMessage::noteOff(channel,playedNote),at-pos);const int key=edge?normal:c.key;send(juce::MidiMessage::noteOn(channel,key,(juce::uint8)100),at-pos);send(juce::MidiMessage::noteOff(channel,key),at-pos);if(c.kind==2)send(juce::MidiMessage::noteOn(channel,playedNote,(juce::uint8)95),at-pos);}}}
    if(pos%(blockSize*4)==0){const auto amount=t<2||t>=5?0:juce::jlimit(0,127,juce::roundToInt((t-2)*127));if(c.kind==3)send(juce::MidiMessage::controllerEvent(channel,1,amount));if(c.kind==4)send(juce::MidiMessage::channelPressureChange(channel,amount));if(c.kind==5&&juce::isPositiveAndBelow(c.param,p.getParameters().size()))p.getParameters()[c.param]->setValueNotifyingHost(amount/127.f);}
   }
   if(!ended&&pos+blockSize>off){send(juce::MidiMessage::noteOff(channel,playedNote),juce::jmax(0,off-pos));ended=true;}
   p.processBlock(a,midi);double sum=0;float bp=0;for(int ch=0;ch<2;++ch)for(int s=0;s<blockSize;++s){const auto v=a.getSample(ch,s);if(!std::isfinite(v))throw std::runtime_error("Nonfinite audio");sum+=v*v;bp=juce::jmax(bp,std::abs(v));}power+=sum;peak=juce::jmax(peak,bp);if(bp>1e-5f)++signal;
   a.applyGain(.125f);if(!writer->writeFromAudioSampleBuffer(a,0,juce::jmin(blockSize,total-pos)))throw std::runtime_error("WAV write failed");*metrics<<juce::String(t,6)<<"\t"<<juce::String(std::sqrt(sum/(2*blockSize)),8)<<"\t"<<juce::String(bp,8)<<"\n";while(juce::Time::getMillisecondCounterHiRes()<renderStarted+(pos+blockSize)*1000.0/rate)pump(1);
  }
  juce::DynamicObject::Ptr result=new juce::DynamicObject;result->setProperty("case",c.id);result->setProperty("note",playedNote);result->setProperty("channel",channel);result->setProperty("peak",peak);result->setProperty("sumSquares",power);result->setProperty("signalBlocks",signal);result->setProperty("sourceAboveUnity",peak>=1);result->setProperty("recordingGain",.125);result->setProperty("recordingClipped",peak>=8);result->setProperty("musicalRetrigger",c.kind==2);result->setProperty("judgement","EVIDENCE_ONLY_NOT_A_SEAMLESS_PASS");json(wav.withFileExtension(".json"),result);
 }
};
}
int main(int argc,char** argv){
 try{const auto args=arguments(argc,argv);argc=args.size();juce::ScopedJuceInitialiser_GUI init;auto exe=juce::File::getSpecialLocation(juce::File::currentExecutableFile).getParentDirectory();
  if(argc==3&&args[1]=="--self-test"){Test t;t.out=juce::File(args[2]);t.out.createDirectory();for(int kind:{0,1,2,3,4}){FakePlugin p;Case c{"self-"+juce::String(kind),kind,26};t.render(p,c,67,7.8,t.out.getChildFile(c.id+".wav"));if(p.starts!=(kind==2?3:1)||p.active)throw std::runtime_error("MIDI schedule regression");}std::cout<<"PASS held groups one musical NoteOn; retrigger reference three; all end released\n";return 0;}
  if(argc<4){std::cerr<<"Usage: EXE plugin.vst3 asset.kam output-directory\n";return 2;}Test test;test.out=juce::File(args[3]);test.out.createDirectory();const juce::String asset=args[2];
  juce::OwnedArray<juce::PluginDescription> descriptions;test.formats.getFormat(0)->findAllTypesForFile(descriptions,args[1]);if(descriptions.isEmpty())throw std::runtime_error("Cannot discover Qin VST3");test.description=*descriptions[0];test.original=fengyin::KongReleaseStates::read(exe.getChildFile("release-states.xml").loadFileAsString(),asset);if(test.original.isEmpty())throw std::runtime_error("Missing captured instrument state");
  auto first=test.load();auto tree=test.state(*first);auto expected=fengyin::KongProjectFile::readPluginComponentState(test.original).getChildWithName("PresetList").getChild(0);auto actual=tree.getChildWithName("PresetList").getChild(0);if(!actual.isValid()||actual.getProperty("KAI").toString()!=expected.getProperty("KAI").toString()||actual.getProperty("Preset").toString()!=expected.getProperty("Preset").toString())throw std::runtime_error("Loaded KAI or preset differs from requested state; refusing mislabeled test");juce::DynamicObject::Ptr inventory=new juce::DynamicObject;inventory->setProperty("plugin",test.description.name);inventory->setProperty("version",test.description.version);inventory->setProperty("asset",asset);juce::Array<juce::var> slots,params;std::vector<Case> cases{{"baseline"},{"cc1-ramp",3},{"channel-pressure-ramp",4}};int slotIndex=0;
  for(auto slot:tree.getChildWithName("PresetList")){juce::DynamicObject::Ptr item=new juce::DynamicObject;for(auto name:{"KAI","Preset","MIDI","KeyswitchLo","KeyswitchHi","IndexSelectedKeyswitch","KeyswitchNames","NoteRanges"})item->setProperty(name,slot.getProperty(name));slots.add(juce::var(item.get()));if(slotIndex++!=0)continue;test.channel=(int)slot.getProperty("MIDI",0)+1;const int lo=slot.getProperty("KeyswitchLo",-1),hi=slot.getProperty("KeyswitchHi",-1);test.normal=lo+(int)slot.getProperty("IndexSelectedKeyswitch",0);if(lo<0||hi>127||test.normal<lo||test.normal>hi)throw std::runtime_error("Invalid keyswitch metadata; refusing guessed MIDI notes");const auto namesValue=slot.getProperty("KeyswitchNames");auto* names=namesValue.getArray();if(names)for(int i=0;i<names->size()&&lo+i<=hi;++i){const auto n=(*names)[i].toString();if(n.startsWithIgnoreCase("Vib")||n=="Pizz"||n=="Flutter"||n=="Flutter2"||n=="Roll"||n=="Sus_Main_Roll"||n=="Main_Shake_Mellow"){cases.push_back({n+"-held",1,lo+i});cases.push_back({n+"-retrigger-reference",2,lo+i});}}}
  if(slotIndex==0)throw std::runtime_error("No loaded slots; check Qin soundbank path and licence");
  for(int i=0;i<first->getParameters().size();++i){auto* p=first->getParameters()[i];const auto name=p->getName(128);juce::DynamicObject::Ptr item=new juce::DynamicObject;item->setProperty("index",i);item->setProperty("name",name);item->setProperty("text",p->getText(p->getValue(),128));params.add(juce::var(item.get()));const auto n=name.toLowerCase().removeCharacters(" ._-");if(n=="vibratodepth"||n=="vibratoamount"||n=="vibratointensity")cases.push_back({"parameter-"+juce::String(i),5,-1,i});}
  inventory->setProperty("slots",slots);inventory->setProperty("parameters",params);inventory->setProperty("automaticConclusion",false);json(test.out.getChildFile("inventory.json"),inventory);first->releaseResources();first.reset();
  // Probe several ordinary pitches, never keyswitch pitches. Keep all evidence.
  double best=-1;for(int note:{60,67,74,81}){auto p=test.load();auto file=test.out.getChildFile("probe-"+juce::String(note)+".wav");test.render(*p,{"probe"},note,1.2,file,true);p->releaseResources();p.reset();auto v=juce::JSON::parse(file.withFileExtension(".json").loadFileAsString());double energy=v.getProperty("sumSquares",0);if(energy>best){best=energy;test.note=note;}}
  if(best<1e-7)throw std::runtime_error("No audible probe: missing samples/licence/wrong routing/unsupported range; NOT an unsupported-technique verdict");
  for(const auto& c:cases){std::cout<<asset<<" "<<c.id<<std::endl;test.out.getChildFile("progress.txt").replaceWithText(c.id);auto p=test.load();test.render(*p,c,test.note,7.8,test.out.getChildFile(c.id+".wav"));p->releaseResources();p.reset();}
  test.out.getChildFile("completed.txt").replaceWithText("Evidence complete. No seamless capability is automatically certified.\n");return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<std::endl;return 1;}
}
