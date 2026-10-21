#include "WindowRecordingService.h"
#include "RecordingPaths.h"
#if JUCE_WINDOWS
#define NOMINMAX
#include <windows.h>
#endif
namespace fengyin {
juce::File WindowRecordingService::audioFolder() const {return baseFolder==juce::File()?RecordingPaths::audio():baseFolder.getChildFile("audio");}
juce::File WindowRecordingService::videoFolder() const {return baseFolder==juce::File()?RecordingPaths::video():baseFolder.getChildFile("video");}
juce::File WindowRecordingService::temporaryFolder() const {return baseFolder==juce::File()?RecordingPaths::temporary():baseFolder.getChildFile("temporary");}
bool WindowRecordingService::createSession() {
    cleanupWarning=false;
    sessionFolder=temporaryFolder().getChildFile(juce::Uuid().toString());
    return sessionFolder.createDirectory().wasOk() && sessionFolder.getChildFile("fengyin-session-v2").replaceWithText("FengYin recording session v2");
}
void WindowRecordingService::cleanCompletedSession() {
    // Only our successful, marked, direct-child session. Never sweep an old or
    // user-selected directory; failed/recoverable originals remain untouched.
    if(sessionFolder.getParentDirectory()!=temporaryFolder() || sessionFolder.isSymbolicLink()
       || sessionFolder.getChildFile("fengyin-session-v2").loadFileAsString()!="FengYin recording session v2")return;
    const juce::StringArray owned{"audio.partial.wav","audio.wav","video.partial.mp4","alignment.txt","metrics.txt","audio-metrics.txt","failure.txt","state.txt","state.tmp","state.txt.tmp","stop","begin"};
    for(const auto& file:sessionFolder.findChildFiles(juce::File::findFiles,false)){
        if(file.isSymbolicLink())continue;
        const auto name=file.getFileName();
        if(owned.contains(name)||name.matchesWildcard("recording-incomplete*.mp4",false))file.deleteFile();
    }
    // A foreign file placed here is not ours to delete. Keep the marker and
    // report residuals rather than recursively sweeping arbitrary contents.
    const auto entries=sessionFolder.findChildFiles(juce::File::findFilesAndDirectories,false);
    if(entries.size()==1&&entries[0].getFileName()=="fengyin-session-v2"){
        entries[0].deleteFile();cleanupWarning=!sessionFolder.deleteFile();
    }else cleanupWarning=true;
}
WindowRecordingService::~WindowRecordingService() {
    stop();
    if(running && !process.waitForProcessToFinish(10000))process.kill();
    lastPollAt=0; // Shutdown must collect completion even inside the 200ms poll interval.
    poll(); // Preserve partial files on failure; never delete user recordings.
}
bool WindowRecordingService::startAudio(bool prepareOnly) {
    poll();if(busy())return false;
    screen=false;captured=false;muxing=false;armed=prepareOnly;cancelled=false;videoQualityWarning=false;
    lastError.clear();completed=false;finishing=false;ready=false;startupGuard.reset();lastPollAt=0;
   #if JUCE_WINDOWS
    const auto executable=juce::File::getSpecialLocation(juce::File::currentApplicationFile)
        .getSiblingFile("FengYinRecorder.exe");
    juce::Logger::writeToLog("Recording backend=miniaudio scope=system-output audio-only");
    const auto folder=audioFolder();
    if(!executable.existsAsFile() || !folder.createDirectory()) {
        lastError=juce::String::fromUTF8("录制组件不可用，或无法创建录制目录");return false;
    }
    destination=RecordingPaths::next(folder,"wav");
    if(!createSession()){lastError=juce::String::fromUTF8("无法创建录制临时目录");return false;}
    partialFile=sessionFolder.getChildFile("audio.partial.wav");
    stopFile=sessionFolder.getChildFile("stop");stateFile=sessionFolder.getChildFile("state.txt");
    juce::StringArray args {executable.getFullPathName(),
        "audio",
        partialFile.getFullPathName(),stopFile.getFullPathName(),stateFile.getFullPathName(),
        "ff000000","0",juce::String(static_cast<juce::int64>(GetCurrentProcessId()))};
    if(prepareOnly)args.add(sessionFolder.getChildFile("begin").getFullPathName());
    running=process.start(args,0);
    startedAt=juce::Time::getMillisecondCounterHiRes();stoppedAt=0;
    if(!running)lastError=juce::String::fromUTF8("无法启动录制组件");
    return running;
   #else
    lastError=juce::String::fromUTF8("录制目前仅支持Windows 10 1903及以上版本");return false;
   #endif
}
bool WindowRecordingService::startScreen(std::uintptr_t window,bool prepareOnly) {
    poll();if(busy())return false;
    screen=true;captured=false;muxing=false;armed=prepareOnly;cancelled=false;completed=false;finishing=false;ready=false;lastError.clear();videoQualityWarning=false;
    startupGuard.reset();
   #if JUCE_WINDOWS
    const auto executable=juce::File::getSpecialLocation(juce::File::currentApplicationFile).getSiblingFile("FengYinScreenCapture.exe");
    const auto folder=videoFolder();
    if(!window || !executable.existsAsFile() || folder.createDirectory().failed()){
        lastError=juce::String::fromUTF8("录屏组件不可用或无法创建目录");return false;
    }
    const auto name=RecordingPaths::next(folder,"mp4");
    destination=name.getSiblingFile(name.getFileNameWithoutExtension()+"-"+juce::Uuid().toString().substring(0,8)+".mp4");
    if(!createSession()){lastError=juce::String::fromUTF8("无法创建录屏临时目录");return false;}
    stopFile=sessionFolder.getChildFile("stop");stateFile=sessionFolder.getChildFile("state.txt");
    partialFile=sessionFolder.getChildFile("recording.mp4");
    juce::StringArray args {executable.getFullPathName(),"--capture",juce::String::toHexString(static_cast<juce::int64>(window)),
        sessionFolder.getFullPathName(),juce::String(static_cast<juce::int64>(GetCurrentProcessId())),"21600"};
    if(prepareOnly)args.add("--armed");
    running=process.start(args,0);startedAt=juce::Time::getMillisecondCounterHiRes();stoppedAt=0;lastPollAt=0;
    if(!running)lastError=juce::String::fromUTF8("无法启动GPU录屏组件");
    return running;
   #else
    (void)window;lastError=juce::String::fromUTF8("录屏仅支持Windows 10 1903及以上版本");return false;
   #endif
}
bool WindowRecordingService::beginCapture() {
    if(!prepared())return false;
    if(!sessionFolder.getChildFile("begin").replaceWithText("begin")){
        lastError=juce::String::fromUTF8("无法开始录制，未保存提示音");stop();return false;
    }
    armed=false;ready=false;startupGuard.reset();startedAt=juce::Time::getMillisecondCounterHiRes();lastPollAt=0;
    return true;
}
bool WindowRecordingService::resumeScreenSession(const juce::File& folder) {
    if(busy())return false;
    if(!folder.getChildFile("alignment.txt").existsAsFile() || !folder.getChildFile("audio.wav").existsAsFile()
        || !folder.getChildFile("video.partial.mp4").existsAsFile()){
        lastError=juce::String::fromUTF8("请选择包含alignment.txt、audio.wav和video.partial.mp4的录屏会话文件夹");return false;
    }
    if(videoFolder().createDirectory().failed()){lastError=juce::String::fromUTF8("无法创建录屏文件目录，原始素材未改动");return false;}
    screen=true;captured=true;muxing=false;armed=false;cancelled=false;cleanupWarning=false;completed=false;finishing=false;ready=false;lastError.clear();sessionFolder=folder;
    videoQualityWarning=folder.getChildFile("metrics.txt").loadFileAsString().contains("qualityWarning=1");
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
        captured=false;completed=true;lastError.clear();cleanCompletedSession();return true;
    }
    if(partialFile.existsAsFile()){
        const auto preserved=sessionFolder.getNonexistentChildFile("recording-incomplete",".mp4");
        if(!partialFile.moveFileTo(preserved)){
            lastError=juce::String::fromUTF8("无法保留上次未完成的合成文件，未覆盖任何文件");captured=false;completed=true;return false;
        }
    }
    juce::StringArray args {executable.getFullPathName(),"--mux-session",sessionFolder.getFullPathName()};
    running=process.start(args,0);
    if(!running){lastError=juce::String::fromUTF8("无法启动视频合成，原始音视频已保留");captured=false;completed=true;return false;}
    lastError.clear();muxing=true;captured=false;finishing=true;ready=false;stoppedAt=juce::Time::getMillisecondCounterHiRes();lastPollAt=0;
    return true;
}
void WindowRecordingService::stop() {
    if(running && !finishing && !muxing){
        // Only an explicit cancellation before begin may discard temporary data.
        // An error or a delayed first frame must never be treated as cancellation.
        cancelled=armed&&lastError.isEmpty();
        finishing=true;
        stoppedAt=juce::Time::getMillisecondCounterHiRes();
        if(!stopFile.replaceWithText("stop")) {
            lastError=juce::String::fromUTF8("无法写入录制停止标记，已停止采集并保留临时文件");
            cancelled=false;
            process.kill();
        }
    }
}
void WindowRecordingService::poll() {
    if(!running)return;
    const auto now=juce::Time::getMillisecondCounterHiRes();
    if(now-lastPollAt<200)return;
    lastPollAt=now;
    auto status=stateFile.loadFileAsString().trim();
    const bool failed=status.startsWith("failed:");
    const bool terminal=failed||status=="saved"||status=="cancelled"||status.startsWith("captured;");
    // Atomic state-file replacement can briefly be unreadable. A successful
    // handshake is latched; missing text is not evidence of a new startup.
    if(status==(armed?"ready":"recording"))ready=true;
    startupGuard.observe(ready);
    if(failed){
        if(lastError.isEmpty()||lastError==juce::String::fromUTF8("录制启动超时，未正常开始采集"))lastError=juce::String::fromUTF8(muxing?"录屏保存失败：":ready&&!armed?"录制中断：":"录制准备或启动失败：")+status.substring(7);
        cancelled=false;
    }
    if(terminal&&!finishing){finishing=true;stoppedAt=now;}
    if(startupGuard.timedOut(now-startedAt,finishing,terminal,process.isRunning())){
        lastError=juce::String::fromUTF8("录制启动超时，未正常开始采集");stop();cancelled=false;
    }
    if(finishing && now-stoppedAt>(muxing?600000:15000) && process.isRunning()) {
        if(lastError.isEmpty())lastError=juce::String::fromUTF8("录制保存超时，已停止采集；如有临时文件将保留");
        cancelled=false;
        process.kill();
    }
    if(process.isRunning())return;
    // The helper may publish its final state between the first read and exit.
    const auto finalStatus=stateFile.loadFileAsString().trim();
    if(finalStatus.isNotEmpty())status=finalStatus;
    if(status.startsWith("failed:")){
        cancelled=false;
        if(lastError.isEmpty()||lastError==juce::String::fromUTF8("录制启动超时，未正常开始采集"))lastError=juce::String::fromUTF8(muxing?"录屏保存失败：":ready&&!armed?"录制中断：":"录制准备或启动失败：")+status.substring(7);
    }
    running=false;finishing=false;ready=false;
    if(cancelled&&process.getExitCode()==0&&status=="cancelled"){
        armed=false;completed=true;lastError.clear();cleanCompletedSession();return;
    }
    cancelled=false;
    if(screen && !muxing && process.getExitCode()==0 && status.startsWith("captured;")
        && sessionFolder.getChildFile("alignment.txt").existsAsFile()){
        const auto metrics=sessionFolder.getChildFile("metrics.txt").loadFileAsString();
        videoQualityWarning=metrics.contains("qualityWarning=1");
        juce::Logger::writeToLog("Screen recording capture metrics:\n"+metrics.substring(0,4096));
        captured=true;lastError.clear();return;
    }
    completed=true;muxing=false;
    if(process.getExitCode()==0 && status=="saved" && partialFile.getSize()>1024 && !destination.existsAsFile()
        && partialFile.moveFileTo(destination)){lastError.clear();cleanCompletedSession();}
    else if(lastError.isEmpty())lastError=juce::String::fromUTF8("录制未成功保存：")
        +(status.isNotEmpty()?status:juce::String::fromUTF8("组件未启动或Windows媒体组件不可用"))
        +juce::String::fromUTF8("。临时素材已保留。");
    if(lastError.isNotEmpty()){
        const auto diagnostic="state="+status+"\nexit="+juce::String(process.getExitCode())+"\nerror="+lastError+"\n";
        sessionFolder.getChildFile("failure.txt").replaceWithText(diagnostic);
        juce::Logger::writeToLog("Recording failure session="+sessionFolder.getFullPathName()+"\n"+diagnostic);
    }
    stopFile.deleteFile(); // Own per-session control marker, not user data.
}
}
