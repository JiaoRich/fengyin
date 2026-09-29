#include <juce_audio_processors/juce_audio_processors.h>
#include <iostream>
#include <optional>
#include <cmath>

using namespace juce;
static std::optional<double> number(String text)
{
    const auto s=text.trim(); char* end=nullptr; const auto n=std::strtod(s.toRawUTF8(),&end);
    if(end==s.toRawUTF8()||!std::isfinite(n))return {};
    const auto suffix=String(end).trim().toLowerCase();
    if(suffix.isNotEmpty()&&suffix!="db"&&suffix!="hz"&&suffix!="ms"&&suffix!="%"&&suffix!="s")return {};
    return n;
}
static bool same(String a,String b)
{
    if(a.trim().equalsIgnoreCase(b.trim()))return true;
    auto x=number(a),y=number(b);return x&&y&&std::abs(*x-*y)<0.000001;
}
struct Match {float value; String method;};
// Query-only: no parameter setter, MIDI event, state restoration or processing.
static std::optional<Match> resolve(AudioProcessorParameter& p,const String& text)
{
    const auto check=[&](float x){return std::isfinite(x)&&x>=0&&x<=1&&same(p.getText(x,1024),text);};
    const float parsed=p.getValueForText(text);
    if(check(parsed))return Match{parsed,"plugin.getValueForText + getText roundtrip"};
    const auto target=number(text),a=number(p.getText(0,1024)),b=number(p.getText(1,1024));
    if(target&&a&&b&&*a!=*b&&*target>=std::min(*a,*b)&&*target<=std::max(*a,*b))
    {
        float lo=0,hi=1;
        for(int k=0;k<48;++k){const float mid=(lo+hi)*.5f;if(check(mid))return Match{mid,"plugin.getText binary search, exact displayed-value match"};
            const auto v=number(p.getText(mid,1024));if(!v)break;
            if((*v<*target)==(*a<*b))lo=mid;else hi=mid;}
    }
    for(int k=0;k<=8192;++k){const float x=k/8192.f;if(check(x))return Match{x,"plugin.getText sampled search, exact displayed-value match"};}
    return {};
}
static String stableId(AudioProcessorParameter& p,int i)
{
    if(auto* named=dynamic_cast<AudioProcessorParameterWithID*>(&p))return "id:"+named->paramID;
    return "index:"+String(i)+":"+p.getName(160).toLowerCase().removeCharacters(" ._-/()[]");
}
static int collect(const StringArray& args)
{
    ScopedJuceInitialiser_GUI initialise;
    const int argc=args.size();
    if(argc<4){std::cerr<<"Usage: SwamParameterCollector plugin.vst3 requests.json output.json [--test-mode]\n";return 2;}
    const bool test=argc>4&&args[4]=="--test-mode";
    const File pluginFile(args[1]),requestFile(args[2]),outputFile(args[3]);
    const auto requests=JSON::parse(requestFile);
    if(!requests.isObject()||requests["schema"].toString()!="fengyin.swam-collection-requests.v1"){std::cerr<<"Invalid requests\n";return 3;}
    std::cout<<"Scanning "<<pluginFile.getFullPathName()<<std::endl;
    AudioPluginFormatManager formats;formats.addFormat(std::make_unique<VST3PluginFormat>());
    OwnedArray<PluginDescription> descriptions;formats.getFormat(0)->findAllTypesForFile(descriptions,pluginFile.getFullPathName());
    if(descriptions.size()!=1){std::cerr<<"Expected one VST3 instrument\n";return 4;}
    auto description=*descriptions[0];
    auto requested=requests[Identifier(description.name)];
    if(!description.name.startsWithIgnoreCase("SWAM")||description.manufacturerName!="Audio Modeling"||!description.isInstrument){std::cerr<<"Not a SWAM instrument\n";return 5;}
    if(!test&&!SystemStats::getOperatingSystemName().containsIgnoreCase("windows")){std::cerr<<"Requires Windows; use explicit test-mode for development\n";return 6;}
    String error;auto plugin=formats.createPluginInstance(description,44100,256,error);
    if(!plugin){std::cerr<<error<<std::endl;return 7;}
    MessageManager::getInstance()->runDispatchLoopUntil(2000);
    plugin->fillInPluginDescription(description);
    auto* root=new DynamicObject();var result(root);
    root->setProperty("schema","fengyin.swam-parameter-collection.v2");
    root->setProperty("collectorVersion","2.0.0");root->setProperty("testOnly",test);
    root->setProperty("platform",SystemStats::getOperatingSystemName());root->setProperty("pluginName",description.name);
    root->setProperty("pluginVersion",description.version);root->setProperty("manufacturer",description.manufacturerName);
    root->setProperty("pluginDescriptionXml",description.createXml()->toString());
    root->setProperty("pluginIdentifier",description.createIdentifierString());root->setProperty("capturedAt",Time::getCurrentTime().toISO8601(true));
    root->setProperty("noParameterSetters",true);root->setProperty("noMidiInputOrAudioDevice",true);
    root->setProperty("scope","Read-only parameter text queries in a separate fresh plugin instance; not a sound preset or a full state export.");
    Array<var> rows,missing,ambiguous;std::vector<float> before;for(auto* p:plugin->getParameters())before.push_back(p->getValue());
    const auto* targets=requested.getDynamicObject();
    if(targets)for(const auto& entry:targets->getProperties())
    {
        int count=0;for(auto* p:plugin->getParameters())if(p->getName(1024).trim()==entry.name.toString())++count;
        if(count==0)missing.add(entry.name.toString());if(count>1)ambiguous.add(entry.name.toString());
    }
    int unresolved=0,sampleCount=0;
    for(int index=0;index<plugin->getParameters().size();++index)
    {
        auto& p=*plugin->getParameters()[index];auto* row=new DynamicObject();
        const auto name=p.getName(1024).trim();
        row->setProperty("nameEnglish",p.getName(1024));row->setProperty("id",stableId(p,index));row->setProperty("index",index);
        row->setProperty("currentNormalisedValue",p.getValue());row->setProperty("currentDisplayValue",p.getCurrentValueAsText());
        row->setProperty("minimumDisplay",p.getText(0,1024));row->setProperty("maximumDisplay",p.getText(1,1024));row->setProperty("unit",p.getLabel());
        row->setProperty("defaultNormalisedValue",p.getDefaultValue());row->setProperty("defaultDisplayValue",p.getText(p.getDefaultValue(),1024));
        row->setProperty("isDiscrete",p.isDiscrete());row->setProperty("numSteps",p.getNumSteps());
        const bool midiProxy=name.startsWithIgnoreCase("MIDI CC");
        int divisions=midiProxy?2:1024;
        if(!midiProxy&&p.isDiscrete()&&p.getNumSteps()>1&&p.getNumSteps()<=1025)divisions=p.getNumSteps()-1;
        Array<var> samples,options;StringArray optionNames;
        for(int k=0;k<=divisions;++k)
        {
            const float n=float(k)/divisions;const auto text=p.getText(n,1024);auto* point=new DynamicObject();
            point->setProperty("normalisedValue",n);point->setProperty("displayValue",text);samples.add(var(point));++sampleCount;
            if(!midiProxy&&!number(text)&&text.trim().isNotEmpty()&&!optionNames.contains(text.trim()))
            {optionNames.add(text.trim());auto* option=new DynamicObject();option->setProperty("name",text.trim());option->setProperty("sampleNormalisedValue",n);options.add(var(option));}
        }
        row->setProperty("samples",samples);row->setProperty("samplingDivisions",divisions);
        row->setProperty("samplingScope",midiProxy?"MIDI proxy: endpoints and midpoint only; not a tone setting":"Observed getText samples; finite sampling does not prove arbitrary-value interpolation or exhaustive enum coverage");
        if(!options.isEmpty())row->setProperty("observedTextOptions",options);
        Array<var> values;
        const var requestValues=targets?targets->getProperty(Identifier(name)):var();
        if(auto* wanted=requestValues.getArray())for(const auto& v:*wanted)
        {
            auto* match=new DynamicObject();match->setProperty("requestedDisplay",v);auto resolved=resolve(p,v.toString());match->setProperty("resolved",bool(resolved));
            if(resolved){match->setProperty("normalisedValue",resolved->value);match->setProperty("verifiedDisplay",p.getText(resolved->value,1024));match->setProperty("method",resolved->method);}
            else ++unresolved;
            values.add(var(match));
        }
        row->setProperty("targets",values);
        if(name=="Instrument")
        {
            StringArray names;Array<var> models;
            for(int i=0;i<=8192;++i){const float n=i/8192.f;const auto text=p.getText(n,1024).trim();if(text.isNotEmpty()&&!names.contains(text)){names.add(text);auto* model=new DynamicObject();model->setProperty("name",text);model->setProperty("sampleNormalisedValue",n);models.add(var(model));}}
            row->setProperty("observedModels",models);
        }
        rows.add(var(row));if(!midiProxy)std::cout<<"Queried "<<name<<std::endl;
    }
    Array<var> drift;
    for(int i=0;i<plugin->getParameters().size();++i)if(plugin->getParameters()[i]->getValue()!=before[static_cast<size_t>(i)])drift.add(stableId(*plugin->getParameters()[i],i));
    root->setProperty("parameters",rows);root->setProperty("missingParameters",missing);root->setProperty("ambiguousParameters",ambiguous);root->setProperty("changedParametersDuringQueries",drift);
    root->setProperty("parameterValuesUnchanged",drift.isEmpty());
    root->setProperty("publicParameterCount",plugin->getParameters().size());root->setProperty("collectedParameterCount",rows.size());
    root->setProperty("sampleCount",sampleCount);root->setProperty("unresolvedRequestedValues",unresolved);
    root->setProperty("hiddenPluginSettingsIncluded",false);
    root->setProperty("versionPolicy","Actual installed version, never relabelled or assumed compatible with other versions");
    const auto parent=outputFile.getParentDirectory();if(!parent.isDirectory()&&!parent.createDirectory())return 8;
    if(!outputFile.replaceWithText(JSON::toString(result)))return 9;
    std::cout<<"SAVED "<<outputFile.getFullPathName()<<std::endl;
    return drift.isEmpty()?0:10;
}
#if JUCE_WINDOWS
int wmain(int argc,wchar_t** argv)
{
    StringArray args;for(int i=0;i<argc;++i)args.add(String(argv[i]));return collect(args);
}
#else
int main(int argc,char** argv)
{
    StringArray args;for(int i=0;i<argc;++i)args.add(String::fromUTF8(argv[i]));return collect(args);
}
#endif
