#include <juce_audio_devices/juce_audio_devices.h>
#include <cassert>
#include <iostream>
class Device final : public juce::AudioIODevice {
public:
 Device():AudioIODevice("Speakers","RetentionTest"){}
 juce::StringArray getOutputChannelNames()override{return {"L","R"};} juce::StringArray getInputChannelNames()override{return {};}
 juce::Array<double> getAvailableSampleRates()override{return {44100.,48000.};} juce::Array<int> getAvailableBufferSizes()override{return {128,256,480};}
 int getDefaultBufferSize()override{return 480;}
 juce::String open(const juce::BigInteger&,const juce::BigInteger& outputs,double rate,int period)override{sampleRate=rate;buffer=period;channels=outputs;opened=true;return {};}
 void close()override{stop();opened=false;}bool isOpen()override{return opened;}
 void start(juce::AudioIODeviceCallback* c)override{callback=c;if(c)c->audioDeviceAboutToStart(this);}void stop()override{if(callback)callback->audioDeviceStopped();callback=nullptr;}
 bool isPlaying()override{return callback!=nullptr;}juce::String getLastError()override{return {};}
 int getCurrentBufferSizeSamples()override{return buffer;}double getCurrentSampleRate()override{return sampleRate;}int getCurrentBitDepth()override{return 32;}
 juce::BigInteger getActiveOutputChannels()const override{return channels;}juce::BigInteger getActiveInputChannels()const override{return {};}
 int getOutputLatencyInSamples()override{return 0;}int getInputLatencyInSamples()override{return 0;}
 bool opened=false;int buffer=0;double sampleRate=0;juce::BigInteger channels;juce::AudioIODeviceCallback* callback=nullptr;
};
class Type final:public juce::AudioIODeviceType {
public:Type():AudioIODeviceType("RetentionTest"){}void scanForDevices()override{}
 juce::StringArray getDeviceNames(bool input=false)const override{return input?juce::StringArray{}:juce::StringArray{"Speakers"};}
 int getDefaultDeviceIndex(bool)const override{return 0;}int getIndexOfDevice(juce::AudioIODevice*,bool)const override{return 0;}
 bool hasSeparateInputsAndOutputs()const override{return true;}
 juce::AudioIODevice* createDevice(const juce::String& output,const juce::String&)override{return output=="Speakers"?new Device:nullptr;}
};
class Manager final:public juce::AudioDeviceManager {void createAudioDeviceTypes(juce::OwnedArray<juce::AudioIODeviceType>& types)override{types.add(new Type);}};
int main(){juce::ScopedJuceInitialiser_GUI gui;
 for(int selected:{128,256})for(double rate:{44100.,48000.}){
  Manager manager;juce::XmlElement saved("DEVICESETUP");saved.setAttribute("deviceType","RetentionTest");saved.setAttribute("audioOutputDeviceName","Removed headphones");saved.setAttribute("audioInputDeviceName","");saved.setAttribute("audioDeviceRate",rate);saved.setAttribute("audioDeviceBufferSize",selected);
  const auto error=manager.initialise(0,2,&saved,true);assert(error.isEmpty());auto* d=manager.getCurrentAudioDevice();assert(d&&d->getName()=="Speakers");assert(d->getCurrentBufferSizeSamples()==selected);assert(d->getCurrentSampleRate()==rate);manager.closeAudioDevice();
 }
 Manager defaults;assert(defaults.initialise(0,2,nullptr,true).isEmpty());assert(defaults.getCurrentAudioDevice()->getCurrentBufferSizeSamples()==480);
 std::cout<<"Removed endpoint fallback preserves 128/256 at 44.1/48kHz; new default still uses 480 only without a saved selection\n";
}
