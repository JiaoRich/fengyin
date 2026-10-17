#include "WindowRecordingService.h"
#include "RecordingPaths.h"
namespace fengyin {
WindowRecordingService::~WindowRecordingService() {
    stop();
    if(running && !process.waitForProcessToFinish(10000))process.kill();
    poll(); // Preserve partial files on failure; never delete user recordings.
}
bool WindowRecordingService::start(void* window,juce::Colour background,bool rounded) {
    poll();if(running)return false;
    lastError.clear();completed=false;finishing=false;ready=false;
   #if JUCE_WINDOWS
    const auto executable=juce::File::getSpecialLocation(juce::File::currentApplicationFile)
        .getSiblingFile("FengYinScreenRecorder.exe");
    const auto folder=RecordingPaths::video();
    if(!window || !executable.existsAsFile() || !folder.createDirectory()) {
        lastError=juce::String::fromUTF8("录屏组件不可用，或无法创建录屏目录");return false;
    }
    destination=RecordingPaths::next(folder,"mp4");
    partialFile=destination.withFileExtension(".partial.mp4");
    stopFile=destination.withFileExtension(".stop");stateFile=destination.withFileExtension(".status");
    juce::StringArray args {executable.getFullPathName(),
        juce::String::toHexString(static_cast<juce::int64>(reinterpret_cast<uintptr_t>(window))),
        partialFile.getFullPathName(),stopFile.getFullPathName(),stateFile.getFullPathName(),
        juce::String::toHexString(static_cast<int>(background.getARGB())),rounded?"1":"0"};
    running=process.start(args,0);
    startedAt=juce::Time::getMillisecondCounterHiRes();stoppedAt=0;
    if(!running)lastError=juce::String::fromUTF8("无法启动录屏组件");
    return running;
   #else
    juce::ignoreUnused(window,background,rounded);
    lastError=juce::String::fromUTF8("录屏目前仅支持Windows 10 1903及以上版本");return false;
   #endif
}
void WindowRecordingService::stop() {
    if(running && !finishing){
        finishing=true;
        stoppedAt=juce::Time::getMillisecondCounterHiRes();
        if(!stopFile.replaceWithText("stop")) {
            lastError=juce::String::fromUTF8("无法写入录屏停止标记，已停止采集并保留临时文件");
            process.kill();
        }
    }
}
void WindowRecordingService::poll() {
    if(!running)return;
    const auto status=stateFile.loadFileAsString().trim();
    ready=status=="recording";
    const auto now=juce::Time::getMillisecondCounterHiRes();
    if(!ready && !finishing && now-startedAt>20000)stop();
    if(finishing && now-stoppedAt>15000 && process.isRunning()) {
        lastError=juce::String::fromUTF8("录屏保存超时，已停止采集；如有临时文件将保留");
        process.kill();
    }
    if(process.isRunning())return;
    running=false;finishing=false;ready=false;completed=true;
    if(process.getExitCode()==0 && status=="saved" && partialFile.getSize()>1024
        && partialFile.moveFileTo(destination))lastError.clear();
    else if(lastError.isEmpty())lastError=juce::String::fromUTF8("录屏未成功保存：")
        +(status.isNotEmpty()?status:juce::String::fromUTF8("组件未启动或Windows媒体组件不可用"))
        +juce::String::fromUTF8("。如有临时文件已保留，请检查磁盘、Windows媒体组件与音频输出设备。");
    stopFile.deleteFile(); // Own per-session control marker, not user data.
}
}
