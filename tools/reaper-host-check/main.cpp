// Standalone diagnostic, not the released application. No audio device is opened.
// Original MIDI is pre-mapped offline. This tests the production host's state
// restore + graph, NOT MainComponent's adaptation or device MIDI receive path.
#include "PluginHostEngine.h"
#include "SwamStateCompatibility.h"
#include <thread>
#include <fstream>
#include <iostream>
#include <cmath>
#include <atomic>
#include <algorithm>
#if JUCE_WINDOWS
 #include <windows.h>
 #include <shellapi.h>
#endif

namespace fengyin { struct PluginHostEngineTestAccess {
 static juce::AudioProcessorGraph* graph(PluginHostEngine& h) { return h.graph.get(); }
}; }
constexpr int rate=48000, blockSize=256;
static void require(bool ok,const char* message) { if(!ok) throw std::runtime_error(message); }
struct Event { int frame; juce::MidiMessage midi; };
struct Head:juce::AudioPlayHead {
 juce::int64 frame=0;
 juce::Optional<PositionInfo> getPosition() const override {
  PositionInfo p;p.setBpm(120);p.setTimeInSamples(frame);p.setTimeInSeconds(double(frame)/rate);
  p.setPpqPosition(double(frame)/rate*2);p.setTimeSignature(TimeSignature{4,4});p.setIsPlaying(true);return p;
 }
};
static juce::File child(const juce::File& parent,const juce::String& name) {
 require(name.isNotEmpty()&&!juce::File::isAbsolutePath(name)&&!name.contains("..")&&!name.containsChar(':'),"Unsafe relative path");
 const auto f=parent.getChildFile(name);require(f.isAChildOf(parent),"Path escapes directory");return f;
}
static void writeJson(const juce::File& file,const juce::var& data) {
 require(file.replaceWithText(juce::JSON::toString(data)),"Cannot write JSON evidence");
}
static std::vector<Event> readMidi(const juce::File& path,int frames) {
 auto stream=path.createInputStream();require(bool(stream),"Missing MIDI input");juce::MidiFile midi;
 require(midi.readFrom(*stream)&&midi.getNumTracks()==1&&midi.getTimeFormat()>0,"Expected one-track PPQ MIDI");
 midi.convertTimestampTicksToSeconds();std::vector<Event> out;
 for(const auto* e:*midi.getTrack(0)) {
  auto m=e->message;if(m.isMetaEvent())continue;
  require(m.getRawDataSize()>=1&&m.getRawDataSize()<=3&&!m.isSysEx(),"Unsupported MIDI event");
  const int frame=juce::roundToInt(m.getTimeStamp()*rate);
  require(frame>=0&&frame<=frames,"MIDI exceeds duration");
  if(frame==frames)continue; // Terminal all-notes-off at end of file has no output frame.
  out.push_back({frame,m});
 }
 require(!out.empty(),"Empty MIDI");
 require(std::is_sorted(out.begin(),out.end(),[](const auto& a,const auto& b){return a.frame<b.frame;}),"MIDI order invalid");
 return out;
}
struct Wave {
 juce::FileOutputStream out;int expected, written=0;double energy=0,peak=0;uint64_t nonFinite=0,fullScale=0;
 Wave(const juce::File& path,int frames):out(path),expected(frames) {
  require(out.openedOk(),"Cannot open WAV output");
  out.write("RIFF",4);out.writeInt(48+frames*8);out.write("WAVEfmt ",8);out.writeInt(16);
  out.writeShort(3);out.writeShort(2);out.writeInt(rate);out.writeInt(rate*8);out.writeShort(8);out.writeShort(32);
  out.write("fact",4);out.writeInt(4);out.writeInt(frames);out.write("data",4);out.writeInt(frames*8);
 }
 void append(const juce::AudioBuffer<float>& x,int n) {
  require(n<=blockSize&&written+n<=expected,"WAV frame overflow");float interleaved[blockSize*2];
  for(int i=0;i<n;++i)for(int c=0;c<2;++c){float v=x.getSample(c,i);interleaved[i*2+c]=v;
   if(!std::isfinite(v)){++nonFinite;continue;}peak=std::max(peak,std::abs(double(v)));energy+=double(v)*v;if(std::abs(v)>=1)++fullScale;
  }
  require(out.write(interleaved,static_cast<size_t>(n)*8),"WAV write failed");written+=n;
 }
 juce::var finish() {
  out.flush();require(out.getStatus().wasOk()&&written==expected,"Incomplete WAV output");
  auto* r=new juce::DynamicObject;r->setProperty("Frames",written);r->setProperty("SampleRate",rate);r->setProperty("Bits",32);r->setProperty("Channels",2);
  r->setProperty("Seconds",double(written)/rate);r->setProperty("Peak",peak);r->setProperty("RMS",std::sqrt(energy/(written*2)));
  r->setProperty("NonFinite",juce::int64(nonFinite));r->setProperty("AtOrAboveFullScale",juce::int64(fullScale));return juce::var(r);
 }
};
template<typename Process>
static juce::var render(const std::vector<Event>& events,int frames,const juce::File& file,Head& head,Process process) {
 require(!file.exists(),"Refusing to overwrite WAV");Wave wav(file,frames);juce::AudioBuffer<float> buffer(2,blockSize);juce::MidiBuffer midi;
 size_t index=0;int count=0;
 for(int at=0;at<frames;at+=blockSize){const int n=std::min(blockSize,frames-at);buffer.setSize(2,n,false,false,true);buffer.clear();midi.clear();
  while(index<events.size()&&events[index].frame<at+n){require(events[index].frame>=at,"Event order broken");midi.addEvent(events[index].midi,events[index].frame-at);++index;++count;}
  head.frame=at;process(buffer,midi);wav.append(buffer,n);
 }
 require(index==events.size(),"MIDI not fully delivered");auto r=wav.finish();r.getDynamicObject()->setProperty("DeliveredMidiEvents",count);return r;
}
static void pump(int ms){juce::MessageManager::getInstance()->runDispatchLoopUntil(ms);}

static int runJob(const juce::File& jobFile) {
 const auto job=juce::JSON::parse(jobFile);require(job.isObject(),"Invalid job JSON");
 const auto root=jobFile.getParentDirectory();const auto reportFile=child(root,job["report"].toString());
 require(!reportFile.exists(),"Refusing to overwrite report");
 auto* data=new juce::DynamicObject;juce::var report(data);data->setProperty("Complete",false);
 data->setProperty("Scope","Production PluginHostEngine loadAsync + graph; pre-mapped MIDI. NOT full FengYin UI/adapter/device queue/effects acceptance.");
 data->setProperty("Job",job["name"]);data->setProperty("Phase","preflight");writeJson(reportFile,report);
 try{
  const auto file=juce::File(job["plugin"].toString());require(file.exists(),"Plugin not found");
  const auto stateFile=child(root,job["state"].toString());juce::MemoryBlock state;
  require(stateFile.getSize()>0&&stateFile.getSize()<64*1024*1024&&stateFile.loadFileAsData(state),"Invalid state file");
  const int frames=static_cast<int>(job["frames"]);require(frames>0&&frames<=rate*60,"Invalid duration");
  const auto events=readMidi(child(root,job["midi"].toString()),frames);
  const auto wavFile=child(root,job["wav"].toString());require(!wavFile.exists(),"Output exists");
  juce::AudioPluginFormatManager fm;fm.addFormat(std::make_unique<juce::VST3PluginFormat>());
  juce::OwnedArray<juce::PluginDescription> descriptions;fm.getFormat(0)->findAllTypesForFile(descriptions,file.getFullPathName());
  juce::PluginDescription* description=nullptr;for(auto* d:descriptions)if(d->name==job["pluginName"].toString())description=d;
  require(description!=nullptr,"Expected plugin name not found");
  data->setProperty("PluginName",description->name);data->setProperty("PluginVersion",description->version);
  require(description->version==job["pluginVersion"].toString(),"Plugin version mismatch");
  data->setProperty("Phase","restore");writeJson(reportFile,report);
  Head head;bool done=false,ok=false;juce::String error;fengyin::PluginHostEngine host;
  host.loadAsync(*description,rate,blockSize,state,[&](bool success,const juce::String& message){ok=success;error=message;done=true;},true);
  const auto deadline=juce::Time::getMillisecondCounterHiRes()+90000;
  while(!done&&juce::Time::getMillisecondCounterHiRes()<deadline)pump(10);
  require(done,"Plugin load timed out");if(!ok)throw std::runtime_error(error.toStdString());
  pump(100);
  const auto restored=host.captureInstrumentState();
  data->setProperty("RestoredStatePreserved",fengyin::SwamStateCompatibility::preserved(state,restored));
  require(bool(data->getProperty("RestoredStatePreserved")),"Restored state differs");
  auto* graph=fengyin::PluginHostEngineTestAccess::graph(host);require(graph&&host.getPlugin(),"Host graph missing");
  graph->setPlayHead(&head);host.getPlugin()->setPlayHead(&head);graph->setNonRealtime(true);host.getPlugin()->setNonRealtime(true);
  graph->prepareToPlay(rate,blockSize);pump(100);
  data->setProperty("PreparedStatePreserved",fengyin::SwamStateCompatibility::preserved(state,host.captureInstrumentState()));
  require(bool(data->getProperty("PreparedStatePreserved")),"Preparing host changed original state");
  data->setProperty("LatencySamples",host.getProcessingLatencySamples());data->setProperty("Phase","render");writeJson(reportFile,report);
  std::atomic<bool> rendered=false;std::exception_ptr failure;juce::var stats;
  std::thread worker([&]{try{stats=render(events,frames,wavFile,head,[&](auto& audio,auto& midi){graph->processBlock(audio,midi);});}catch(...){failure=std::current_exception();}rendered.store(true);});
  while(!rendered.load())pump(10);worker.join();if(failure)std::rethrow_exception(failure);
  data->setProperty("Wav",stats);
  require(int(stats["NonFinite"])==0&&int(stats["AtOrAboveFullScale"])==0,"Nonfinite or full-scale samples detected");
  require(double(stats["Peak"])>0.00001,"Silent rendering");
  graph->releaseResources();host.getPlugin()->setPlayHead(nullptr);graph->setPlayHead(nullptr);host.unload();
  data->setProperty("Phase","complete");data->setProperty("Complete",true);writeJson(reportFile,report);
  std::cout<<"PASS "<<job["name"].toString()<<" complete-state restore and isolated production graph render"<<std::endl;return 0;
 }catch(const std::exception& e){data->setProperty("Error",juce::String::fromUTF8(e.what()));writeJson(reportFile,report);throw;}
}
static int selfTest(const juce::File& folder) {
 require(!folder.exists(),"Self-test directory already exists");require(folder.createDirectory().wasOk(),"Cannot create self-test folder");
 const auto at=[&](int frame,const juce::MidiMessage& m){return Event{frame,m};};
 std::vector<Event> events{at(0,juce::MidiMessage::controllerEvent(1,2,64)),at(0,juce::MidiMessage::controllerEvent(1,34,1)),
  at(255,juce::MidiMessage::noteOn(1,67,juce::uint8(80))),at(256,juce::MidiMessage::controllerEvent(1,34,2)),at(511,juce::MidiMessage::noteOff(1,67))};
 // 24000 PPQ at 120 BPM yields exactly one tick per audio sample.
 juce::MidiMessageSequence sequence;sequence.addEvent(juce::MidiMessage::tempoMetaEvent(500000));
 for(auto e:events){e.midi.setTimeStamp(e.frame);sequence.addEvent(e.midi);}
 juce::MidiFile midi;midi.setTicksPerQuarterNote(24000);midi.addTrack(sequence);
 const auto midiFile=folder.getChildFile("timing.mid");auto midiOut=midiFile.createOutputStream();
 require(midiOut&&midi.writeTo(*midiOut),"MIDI fixture write failed");midiOut.reset();
 const auto parsed=readMidi(midiFile,777);require(parsed.size()==events.size(),"MIDI parsing lost events");
 for(size_t i=0;i<events.size();++i)require(parsed[i].frame==events[i].frame&&parsed[i].midi.getRawDataSize()==events[i].midi.getRawDataSize()
  &&std::memcmp(parsed[i].midi.getRawData(),events[i].midi.getRawData(),size_t(events[i].midi.getRawDataSize()))==0,"MIDI parsing changed sample timing or paired CC ordering");
 std::vector<int> received;Head head;double value=0;const auto file=folder.getChildFile(juce::String::fromUTF8("测试 空格.wav"));
 const auto stats=render(events,777,file,head,[&](auto& audio,auto& midi){for(auto m:midi){received.push_back(int(head.frame)+m.samplePosition);value+=.01;}for(int c=0;c<2;++c)for(int i=0;i<audio.getNumSamples();++i)audio.setSample(c,i,float(value));});
 require(received==std::vector<int>({0,0,255,256,511}),"Sample timing mismatch");
 require(int(stats["Frames"])==777&&int(stats["DeliveredMidiEvents"])==5&&file.getSize()==56+777*8,"WAV/self-test count mismatch");
 require(std::abs(double(stats["Peak"])-.05)<.000001,"Audio generator mismatch");
 bool rejected=false;try{child(folder,"../escape");}catch(...){rejected=true;}require(rejected,"Unsafe path accepted");
 rejected=false;try{render(events,777,file,head,[](auto&,auto&){});}catch(...){rejected=true;}require(rejected,"Output overwrite accepted");
 writeJson(folder.getChildFile("self-test.json"),stats);std::cout<<"PASS sample timing/paired CC ordering/cross-block delivery/WAV/Unicode paths/path bounds/no overwrite"<<std::endl;return 0;
}
int main(int argc,char** argv) {
 try{juce::StringArray args;
 #if JUCE_WINDOWS
 int count=0;auto** wide=CommandLineToArgvW(GetCommandLineW(),&count);require(wide!=nullptr,"Cannot parse command line");for(int i=1;i<count;++i)args.add(juce::String(wide[i]));LocalFree(wide);juce::ignoreUnused(argc,argv);
 #else
 for(int i=1;i<argc;++i)args.add(juce::String::fromUTF8(argv[i]));
 #endif
 require(args.size()==2,"Usage: FengYinHostCheck --job job.json | --self-test new-output-directory");
 juce::ScopedJuceInitialiser_GUI gui;const juce::File file=juce::File::getCurrentWorkingDirectory().getChildFile(args[1]);
 if(args[0]=="--self-test")return selfTest(file);if(args[0]=="--job")return runJob(file);throw std::runtime_error("Unknown mode");
 }catch(const std::exception& e){std::cerr<<"FAILED: "<<e.what()<<std::endl;return 1;}
}
