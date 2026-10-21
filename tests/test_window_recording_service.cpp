#include "../native/Source/WindowRecordingService.h"
#include <stdexcept>
using fengyin::WindowRecordingService;
void require(bool value){if(!value)throw std::runtime_error("Recording lifecycle regression");}
template<class Predicate>void wait(WindowRecordingService& service,Predicate predicate){
    for(int n=0;n<300;++n){service.poll();if(predicate())return;juce::Thread::sleep(20);}
    throw std::runtime_error("Recording lifecycle timeout: "+service.error().toStdString());
}
int main(){
    const auto root=juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("fy-service-"+juce::Uuid().toString());
    require(root.createDirectory().wasOk());
    {
        WindowRecordingService service(root);
        require(service.startAudio());require(!service.startScreen(1));
        wait(service,[&]{return service.recording();});service.stop();
        wait(service,[&]{return !service.busy();});require(service.takeCompleted());require(service.error().isEmpty());
        require(service.lastFile().getSize()==4096);
    }
    juce::File session;
    {
        WindowRecordingService service(root);require(service.startScreen(1));
        wait(service,[&]{return service.recording();});service.stop();
        wait(service,[&]{return service.needsVideoGeneration();});
        require(service.busy()&&!service.recording()&&!service.stopping());require(!service.takeCompleted());
        auto directories=root.getChildFile("video").findChildFiles(juce::File::findDirectories,false);require(directories.size()==1);session=directories[0];
        require(!session.getChildFile("recording.mp4").existsAsFile()); // no automatic encode
    }
    {
        WindowRecordingService resumed(root);require(resumed.resumeScreenSession(session));
        require(session.getChildFile("fail-once").replaceWithText("fail"));
        require(resumed.generateVideo());wait(resumed,[&]{return !resumed.busy();});
        require(resumed.takeCompleted()&&resumed.error().isNotEmpty());
        require(session.getChildFile("audio.wav").getSize()==4096 && session.getChildFile("video.partial.mp4").getSize()==4096);
        require(resumed.resumeScreenSession(session));require(resumed.generateVideo());
        wait(resumed,[&]{return !resumed.busy();});require(resumed.takeCompleted()&&resumed.error().isEmpty());
        require(resumed.lastFile().getSize()==4096);
        require(!session.findChildFiles(juce::File::findFiles,false,"recording-incomplete*.mp4").isEmpty());
        require(session.getChildFile("audio.wav").existsAsFile());
    }
    require(root.deleteRecursively()); // only this test's fresh UUID temp directory
}
