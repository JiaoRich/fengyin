#include "WindowRecordingService.h"
#include "RecordingPaths.h"
#if JUCE_WINDOWS
#define NOMINMAX
#include <windows.h>
#endif
namespace fengyin {
juce::File WindowRecordingService::audioFolder() const {return baseFolder==juce::File()?RecordingPaths::audio():baseFolder.getChildFile("audio");}
juce::File WindowRecordingService::videoFolder() const {return baseFolder==juce::File()?RecordingPaths::video():baseFolder.getChildFile("video");}
WindowRecordingService::~WindowRecordingService() {
    stop();
    if(running && !process.waitForProcessToFinish(10000))process.kill();
    lastPollAt=0; // Shutdown must collect completion even inside the 200ms poll interval.
    poll(); // Preserve partial files on failure; never delete user recordings.
}
bool WindowRecordingService::startAudio() {
    poll();if(busy())return false;
    screen=false;captured=false;muxing=false;
    lastError.clear();completed=false;finishing=false;ready=false;
   #if JUCE_WINDOWS
    const auto executable=juce::File::getSpecialLocation(juce::File::currentApplicationFile)
        .getSiblingFile("FengYinRecorder.exe");
    juce::Logger::writeToLog("Recording backend=miniaudio scope=system-output audio-only");
    const auto folder=audioFolder();
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
bool WindowRecordingService::startScreen(std::uintptr_t window) {
    poll();if(busy())return false;
    screen=true;captured=false;muxing=false;completed=false;finishing=false;ready=false;lastError.clear();
   #if JUCE_WINDOWS
    const auto executable=juce::File::getSpecialLocation(juce::File::currentApplicationFile).getSiblingFile("FengYinScreenCapture.exe");
    const auto folder=videoFolder();
    if(!window || !executable.existsAsFile() || folder.createDirectory().failed()){
        lastError=juce::String::fromUTF8("录屏组件不可用或无法创建目录");return false;
    }
    const auto name=RecordingPaths::next(folder,"mp4");
    destination=name.getSiblingFile(name.getFileNameWithoutExtension()+"-"+juce::Uuid().toString().substring(0,8)+".mp4");
    sessionFolder=folder.getChildFile(destination.getFileNameWithoutExtension()+"-"+juce::Uuid().toString().substring(0,8));
    stopFile=sessionFolder.getChildFile("stop");stateFile=sessionFolder.getChildFile("state.txt");
    partialFile=sessionFolder.getChildFile("recording.mp4");
    juce::StringArray args {executable.getFullPathName(),"--capture",juce::String::toHexString(static_cast<juce::int64>(window)),
        sessionFolder.getFullPathName(),juce::String(static_cast<juce::int64>(GetCurrentProcessId())),"21600"};
    running=process.start(args,0);startedAt=juce::Time::getMillisecondCounterHiRes();stoppedAt=0;lastPollAt=0;
    if(!running)lastError=juce::String::fromUTF8("无法启动GPU录屏组件");
    return running;
   #else
    (void)window;lastError=juce::String::fromUTF8("录屏仅支持Windows 10 1903及以上版本");return false;
   #endif
}
bool WindowRecordingService::resumeScreenSession(const juce::File& folder) {
    if(busy())return false;
    if(!folder.getChildFile("alignment.txt").existsAsFile() || !folder.getChildFile("audio.wav").existsAsFile()
        || !folder.getChildFile("video.partial.mp4").existsAsFile()){
        lastError=juce::String::fromUTF8("请选择包含alignment.txt、audio.wav和video.partial.mp4的录屏会话文件夹");return false;
    }
    screen=true;captured=true;muxing=false;completed=false;finishing=false;ready=false;lastError.clear();sessionFolder=folder;
    stateFile=folder.getChildFile("state.txt");stopFile=folder.getChildFile("stop");
    partialFile=folder.getChildFile("recording.mp4");
    destination=RecordingPaths::next(videoFolder(),"mp4");return true;
}
bool WindowRecordingService::generateVideo() {
    if(!captured || running)return false;
    const auto executable=juce::File::getSpecialLocation(juce::File::currentApplicationFile).getSiblingFile("FengYinScreenCapture.exe");
    // The helper never overwrites an existing output. Successful previous mux
    // can be recovered without re-encoding or losing the original WAV/video.
    if(stateFile.loadFileAsString().trim()=="saved" && partialFile.getSize()>1024 && !destination.existsAsFile()
        && partialFile.moveFileTo(destination)){
        captured=false;completed=true;lastError.clear();return true;
    }
    if(partialFile.existsAsFile()){
        const auto preserved=sessionFolder.getNonexistentChildFile("recording-incomplete",".mp4");
        if(!partialFile.moveFileTo(preserved)){
            lastError=juce::String::fromUTF8("无法保留上次未完成的合成文件，未覆盖任何文件");return false;
        }
    }
    juce::StringArray args {executable.getFullPathName(),"--mux-session",sessionFolder.getFullPathName()};
    running=process.start(args,0);
    if(!running){lastError=juce::String::fromUTF8("无法启动视频合成，原始音视频已保留");return false;}
    lastError.clear();muxing=true;captured=false;finishing=true;ready=false;stoppedAt=juce::Time::getMillisecondCounterHiRes();lastPollAt=0;
    return true;
}
void WindowRecordingService::stop() {
    if(running && !finishing && !muxing){
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
    const auto now=juce::Time::getMillisecondCounterHiRes();
    if(now-lastPollAt<200)return;
    lastPollAt=now;
    const auto status=stateFile.loadFileAsString().trim();
    ready=status=="recording";
    if(!ready && !finishing && now-startedAt>20000)stop();
    if(finishing && now-stoppedAt>(muxing?600000:15000) && process.isRunning()) {
        lastError=juce::String::fromUTF8("录制保存超时，已停止采集；如有临时文件将保留");
        process.kill();
    }
    if(process.isRunning())return;
    running=false;finishing=false;ready=false;
    if(screen && !muxing && process.getExitCode()==0 && status.startsWith("captured;")
        && sessionFolder.getChildFile("alignment.txt").existsAsFile()){
        captured=true;lastError.clear();return;
    }
    completed=true;muxing=false;
    if(process.getExitCode()==0 && status=="saved" && partialFile.getSize()>1024 && !destination.existsAsFile()
        && partialFile.moveFileTo(destination))lastError.clear();
    else if(lastError.isEmpty())lastError=juce::String::fromUTF8("录制未成功保存：")
        +(status.isNotEmpty()?status:juce::String::fromUTF8("组件未启动或Windows媒体组件不可用"))
        +juce::String::fromUTF8("。如有临时文件已保留，请检查磁盘、Windows媒体组件与音频输出设备。");
    stopFile.deleteFile(); // Own per-session control marker, not user data.
}
}
