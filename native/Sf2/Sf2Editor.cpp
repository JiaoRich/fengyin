#include "Sf2Processor.h"
#include "VideoFileChooser.h"

namespace fengyin::sf2
{
class Editor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit Editor(Processor& p) : AudioProcessorEditor(p), processor(p),
        // In a Windows VST3, currentExecutableFile is the plug-in DLL, not
        // FengYin.exe. The isolated native picker must run the host's entry.
        picker(*this, juce::File::getSpecialLocation(juce::File::hostApplicationPath), true)
    {
        title.setText(juce::String::fromUTF8("sf2格式音源"), juce::dontSendNotification);
        title.setFont(juce::FontOptions(22.f));
        status.setJustificationType(juce::Justification::centredLeft);
        detail.setText(juce::String::fromUTF8("气息：CC11 · MIDI 通道：1\n混响与其他效果可在调音师效果链中添加。"), juce::dontSendNotification);
        choose.setButtonText(juce::String::fromUTF8("选择 SF2 文件"));
        cancel.setButtonText(juce::String::fromUTF8("取消加载"));
        presetBox.setTextWhenNothingSelected(juce::String::fromUTF8("请选择音色预设"));
        for (auto* c : std::initializer_list<juce::Component*>{&title,&status,&detail,&choose,&cancel,&presetBox,&volume,&bend,&volumeName,&bendName}) addAndMakeVisible(c);
        volumeName.setText(juce::String::fromUTF8("方案音量"), juce::dontSendNotification);
        bendName.setText(juce::String::fromUTF8("弯音范围（半音）"), juce::dontSendNotification);
        for (auto* s : {&volume,&bend}) { s->setSliderStyle(juce::Slider::LinearHorizontal); s->setTextBoxStyle(juce::Slider::TextBoxRight, false, 72, 28); }
        volume.setRange(0,2,.01); bend.setRange(0,24,1);
        volumeAttachment = std::make_unique<juce::SliderParameterAttachment>(*dynamic_cast<juce::RangedAudioParameter*>(p.getParameters()[0]),volume);
        bendAttachment = std::make_unique<juce::SliderParameterAttachment>(*dynamic_cast<juce::RangedAudioParameter*>(p.getParameters()[1]),bend);
        choose.onClick = [this] { pickerError.clear(); picker.open([safe=juce::Component::SafePointer<Editor>(this)](juce::File f) {
            if (safe && f.existsAsFile()) safe->processor.chooseFile(f);
        }, [safe=juce::Component::SafePointer<Editor>(this)](juce::String error) { if (safe) safe->pickerError=error; }); };
        cancel.onClick = [&] { processor.cancelLoad(); };
        presetBox.onChange = [&] { const auto list=processor.availablePresets(); if(auto* a=list.getArray()) {
            const int index=presetBox.getSelectedItemIndex(); if(index>=0 && index<a->size()) processor.choosePreset(int((*a)[index]["bank"]),int((*a)[index]["program"]));
        }};
        setSize(560,350); setResizable(true,false); setResizeLimits(420,350,900,600);
        startTimer(200); timerCallback();
    }
    ~Editor() override { stopTimer(); processor.cancelLoad(); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colour(0xff172b3e)); }
    void resized() override {
        auto area=getLocalBounds().reduced(24); title.setBounds(area.removeFromTop(34));
        area.removeFromTop(12); auto buttons=area.removeFromTop(36); choose.setBounds(buttons.removeFromLeft(180)); buttons.removeFromLeft(12); cancel.setBounds(buttons.removeFromLeft(110));
        area.removeFromTop(10); status.setBounds(area.removeFromTop(36)); presetBox.setBounds(area.removeFromTop(30)); area.removeFromTop(10);
        auto row=area.removeFromTop(38); volumeName.setBounds(row.removeFromLeft(120)); volume.setBounds(row);
        row=area.removeFromTop(38); bendName.setBounds(row.removeFromLeft(120)); bend.setBounds(row);
        area.removeFromTop(10); detail.setBounds(area);
    }
private:
    void timerCallback() override {
        status.setText(pickerError.isEmpty()?processor.statusText():pickerError,juce::dontSendNotification);
        const auto list=processor.availablePresets(); const auto encoded=juce::JSON::toString(list);
        if(encoded!=lastPresets) {
            lastPresets=encoded; presetBox.clear(juce::dontSendNotification);
            if(auto* a=list.getArray()) for(int i=0;i<a->size();++i) presetBox.addItem((*a)[i]["name"].toString()+" ("+(*a)[i]["bank"].toString()+":"+(*a)[i]["program"].toString()+")",i+1);
            presetBox.setVisible(presetBox.getNumItems()>1);
        }
        const auto state=processor.editorSelection(); presetBox.setSelectedId(0,juce::dontSendNotification); if(auto* a=list.getArray()) for(int i=0;i<a->size();++i)
            if(int((*a)[i]["bank"])==state.bank && int((*a)[i]["program"])==state.program) presetBox.setSelectedItemIndex(i,juce::dontSendNotification);
    }
    Processor& processor;
    VideoFileChooser picker;
    juce::Label title,status,detail,volumeName,bendName;
    juce::TextButton choose,cancel;
    juce::ComboBox presetBox;
    juce::Slider volume,bend;
    juce::String lastPresets,pickerError;
    std::unique_ptr<juce::SliderParameterAttachment> volumeAttachment,bendAttachment;
};
juce::AudioProcessorEditor* Processor::createEditor() { return new Editor(*this); }
}
