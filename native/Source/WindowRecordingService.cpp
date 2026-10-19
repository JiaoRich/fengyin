#include "WindowRecordingService.h"
#include "RecordingPaths.h"
#if JUCE_WINDOWS
#define NOMINMAX
#include <windows.h>
#endif
namespace fengyin {
WindowRecordingService::~WindowRecordingService() {
    stop();
    if(running && !process.waitForProcessToFinish(10000))process.kill();
    poll(); // Preserve partial files on failure; never delete user recordings.
}
bool WindowRecordingService::startAudio() {
    poll();if(running)return false;
    lastError.clear();completed=false;finishing=false;ready=false;
   #if JUCE_WINDOWS
    const auto executable=juce::File::getSpecialLocation(juce::File::currentApplicationFile)
        .getSiblingFile("FengYinRecorder.exe");
    juce::Logger::writeToLog("Recording backend=miniaudio scope=system-output audio-only");
    const auto folder=RecordingPaths::audio();
    if(!executable.existsAsFile() || !folder.createDirectory()) {
        lastError=juce::String::fromUTF8("录制组件不可用，或无法创建录制目录");return false;
    }
    destination=RecordingPaths::next(folder,"wav");
    partialFile=destination.withFileExtension(".partial.wav");
    stopFile=destination.withFileExtension(".stop");stateFile=destination.withFileExtension(".status");
    juce::StringArray args {executable.getFullPathName(),
        "audio",
        partialFile.getFullPathName(),stopFile.getFullPathName(),stateFile.getFullPathName(),
        "ff000000","0",juce::String(static_cast<juce::int64>(GetCurrentProcessId()))};
    running=process.start(args,0);
    startedAt=juce::Time::getMillisecondCounterHiRes();stoppedAt=0;
    if(!running)lastError=juce::String::fromUTF8("无法启动录制组件");
    return running;
   #else
    lastError=juce::String::fromUTF8("录制目前仅支持Windows 10 1903及以上版本");return false;
   #endif
}
void WindowRecordingService::stop() {
    if(running && !finishing){
        finishing=true;
        stoppedAt=juce::Time::getMillisecondCounterHiRes();
        if(!stopFile.replaceWithText("stop")) {
            lastError=juce::String::fromUTF8("无法写入录制停止标记，已停止采集并保留临时文件");
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
        lastError=juce::String::fromUTF8("录制保存超时，已停止采集；如有临时文件将保留");
        process.kill();
    }
    if(process.isRunning())return;
    running=false;finishing=false;ready=false;completed=true;
    if(process.getExitCode()==0 && status=="saved" && partialFile.getSize()>1024
        && partialFile.moveFileTo(destination))lastError.clear();
    else if(lastError.isEmpty())lastError=juce::String::fromUTF8("录制未成功保存：")
        +(status.isNotEmpty()?status:juce::String::fromUTF8("组件未启动或Windows媒体组件不可用"))
        +juce::String::fromUTF8("。如有临时文件已保留，请检查磁盘、Windows媒体组件与音频输出设备。");
    stopFile.deleteFile(); // Own per-session control marker, not user data.
}
}
