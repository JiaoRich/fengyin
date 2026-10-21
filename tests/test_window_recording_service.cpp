#include "../native/Source/WindowRecordingService.h"
#include <stdexcept>
#include <iostream>
using fengyin::WindowRecordingService;
void check(bool value,int line){if(!value)throw std::runtime_error("Recording lifecycle regression at line "+std::to_string(line));}
#define require(value) check((value),__LINE__)
template<class Predicate>void wait(WindowRecordingService& service,Predicate predicate){
    for(int n=0;n<300;++n){service.poll();if(predicate())return;juce::Thread::sleep(20);}
    throw std::runtime_error("Recording lifecycle timeout: "+service.error().toStdString());
}
int main() try {
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("fy-service-"+juce::Uuid().toString());
    require(root.createDirectory().wasOk());
    juce::File exitAudio;
    {
        WindowRecordingService service(root);require(service.startAudio());
        wait(service,[&]{return service.recording();});exitAudio=service.lastFile();
        // Destructor must stop, wait, then force a final poll even if the last
        // UI poll was <200ms ago. No orphan partial WAV on normal app exit.
    }
    require(exitAudio.getSize()==4096);
    {
        WindowRecordingService armed(root);require(armed.startAudio(true));
        wait(armed,[&]{return armed.prepared();});require(!armed.recording());
        armed.stop();wait(armed,[&]{return !armed.busy();});require(armed.wasCancelled());
        require(!armed.lastFile().exists());
        require(root.getChildFile("temporary").findChildFiles(juce::File::findDirectories,false).isEmpty());
    }
    {
        WindowRecordingService service(root);
        require(service.startAudio());require(!service.startScreen(1));
        wait(service,[&]{return service.recording();});
        const auto directories=root.getChildFile("temporary").findChildFiles(juce::File::findDirectories,false);
        require(directories.size()==1);require(directories[0].getChildFile("state.txt").deleteFile());
        juce::Thread::sleep(250);service.poll();require(service.recording());
        service.stop();
        wait(service,[&]{return !service.busy();});require(service.takeCompleted());require(service.error().isEmpty());
        require(service.lastFile().getSize()==4096);
    }
    juce::File session;
    {
        WindowRecordingService failing(root);require(failing.startScreen(1));
        wait(failing,[&]{return failing.recording();});
        auto dirs=root.getChildFile("temporary").findChildFiles(juce::File::findDirectories,false);
        require(dirs.size()==1);const auto failedSession=dirs[0];
        // Real elapsed-time regression: the original code mislabelled this
        // failure as startup timeout after a perfectly valid recording start.
        for(int n=0;n<105;++n){juce::Thread::sleep(200);failing.poll();require(failing.recording());}
        require(failedSession.getChildFile("inject-failure").replaceWithText("fail"));
        wait(failing,[&]{return !failing.busy();});
        require(failing.takeCompleted()&&!failing.wasCancelled());
        require(failing.error().contains("injected encoder failure"));
        require(!failing.error().contains(juce::String::fromUTF8("启动超时")));
        require(failedSession.getChildFile("audio.wav").getSize()==4096);
        require(failedSession.getChildFile("failure.txt").existsAsFile());
        require(failedSession.deleteRecursively()); // UUID test fixture only
    }
    {
        WindowRecordingService failing(root);require(failing.startScreen(1,true));
        wait(failing,[&]{return failing.prepared();});
        const auto dirs=root.getChildFile("temporary").findChildFiles(juce::File::findDirectories,false);
        require(dirs.size()==1);require(dirs[0].getChildFile("inject-failure").replaceWithText("fail"));
        wait(failing,[&]{return !failing.busy();});
        require(failing.takeCompleted()&&!failing.wasCancelled());
        require(failing.error().contains("injected preparation failure"));
        require(dirs[0].getChildFile("failure.txt").existsAsFile());
        require(dirs[0].deleteRecursively());
    }
    {
        WindowRecordingService service(root);require(service.startScreen(1,true));
        wait(service,[&]{return service.prepared();});require(!service.recording());
        require(service.beginCapture());
        wait(service,[&]{return service.recording();});service.stop();
        wait(service,[&]{return service.needsVideoGeneration();});
        require(service.busy()&&!service.recording()&&!service.stopping());require(!service.takeCompleted());
        auto directories=root.getChildFile("temporary").findChildFiles(juce::File::findDirectories,false);require(directories.size()==1);session=directories[0];
        require(root.getChildFile("video").findChildFiles(juce::File::findDirectories,false).isEmpty());
        require(!session.getChildFile("recording.mp4").existsAsFile()); // no automatic encode
    }
    {
        require(session.getChildFile("metrics.txt").replaceWithText("qualityWarning=1\nlate=12\n"));
        WindowRecordingService resumed(root);require(resumed.resumeScreenSession(session));
        require(!root.getChildFile("audio").getChildFile(resumed.lastFile().getFileNameWithoutExtension()+".wav").exists());
        require(resumed.hasVideoQualityWarning());
        require(session.getChildFile("fail-once").replaceWithText("fail"));
        require(resumed.generateVideo());wait(resumed,[&]{return !resumed.busy();});
        require(resumed.takeCompleted()&&resumed.error().isNotEmpty());
        require(session.getChildFile("audio.wav").getSize()==4096 && session.getChildFile("video.partial.mp4").getSize()==4096);
        require(resumed.resumeScreenSession(session));require(resumed.generateVideo());
        wait(resumed,[&]{return !resumed.busy();});require(resumed.takeCompleted()&&resumed.error().isEmpty());
        require(resumed.lastFile().getSize()==4096);
        require(root.getChildFile("audio").getChildFile(resumed.lastFile().getFileNameWithoutExtension()+".wav").getSize()==4096);
        require(resumed.hasVideoQualityWarning()); // successful save retains the quality warning
        require(!session.exists()); // successful owned session is automatically cleaned
    }
    {
        // A user-selected recovery directory is never recursively cleaned.
        const auto external=root.getChildFile("user-recovery");require(external.createDirectory().wasOk());
        require(external.getChildFile("audio.wav").replaceWithText(juce::String::repeatedString("a",4096)));
        require(external.getChildFile("video.partial.mp4").replaceWithText(juce::String::repeatedString("v",4096)));
        require(external.getChildFile("alignment.txt").replaceWithText("0 20000000"));
        WindowRecordingService recovery(root);require(recovery.resumeScreenSession(external));
        require(!recovery.hasVideoQualityWarning());require(recovery.generateVideo());
        wait(recovery,[&]{return !recovery.busy();});require(recovery.error().isEmpty());
        require(external.getChildFile("audio.wav").existsAsFile());
    }
    {
        const auto external=root.getChildFile("pair-collision");require(external.createDirectory().wasOk());
        require(external.getChildFile("audio.wav").replaceWithText(juce::String::repeatedString("a",4096)));
        require(external.getChildFile("video.partial.mp4").replaceWithText(juce::String::repeatedString("v",4096)));
        require(external.getChildFile("alignment.txt").replaceWithText("0 20000000"));
        WindowRecordingService recovery(root);require(recovery.resumeScreenSession(external));
        const auto conflict=root.getChildFile("audio").getChildFile(recovery.lastFile().getFileNameWithoutExtension()+".wav");
        require(conflict.replaceWithText("do not overwrite"));
        require(recovery.generateVideo());wait(recovery,[&]{return !recovery.busy();});
        require(recovery.error().isNotEmpty()&&!recovery.lastFile().exists());
        require(conflict.loadFileAsString()=="do not overwrite");
        require(external.getChildFile("recording.mp4").getSize()==4096);
        require(external.getChildFile("recording.wav").getSize()==4096);
        require(conflict.deleteFile());require(recovery.resumeScreenSession(external));
        require(recovery.generateVideo());require(!recovery.busy()&&recovery.error().isEmpty());
        require(recovery.lastFile().getSize()==4096);
        require(root.getChildFile("audio").getChildFile(recovery.lastFile().getFileNameWithoutExtension()+".wav").getSize()==4096);
    }
    require(root.deleteRecursively()); // only this test's fresh UUID temp directory
} catch(const std::exception& error) {
    std::cerr<<error.what()<<std::endl;return 1;
}
