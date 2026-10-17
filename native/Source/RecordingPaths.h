#pragma once
#include <juce_core/juce_core.h>
namespace fengyin {
struct RecordingPaths {
    static juce::File root() {
        return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
            .getChildFile(juce::String::fromUTF8("风吟录制"));
    }
    static juce::File audio(){return root().getChildFile(juce::String::fromUTF8("录音"));}
    static juce::File video(){return root().getChildFile(juce::String::fromUTF8("视频录制"));}
    static bool ensureFolders(){return audio().createDirectory().wasOk() && video().createDirectory().wasOk();}
    static juce::File next(const juce::File& folder,const juce::String& extension,const juce::String& date={}) {
        const auto day=date.isNotEmpty()?date.substring(juce::jmax(0,date.length()-6)):juce::Time::getCurrentTime().formatted("%y%m%d");
        const auto prefix=juce::String::fromUTF8("风吟录制-")+day+"-";
        for(int sequence=1;sequence<10000;++sequence) {
            const auto file=folder.getChildFile(prefix+juce::String(sequence)+"."+extension);
            if(!file.exists() && !file.withFileExtension(".partial.mp4").exists()
                && !file.withFileExtension(".partial.wav").exists()
                && !file.withFileExtension(".status").exists() && !file.withFileExtension(".stop").exists())return file;
        }
        return folder.getChildFile(prefix+juce::Uuid().toString()+"."+extension);
    }
};
}
