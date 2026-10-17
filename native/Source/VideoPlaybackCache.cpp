#include "VideoPlaybackCache.h"
#include <algorithm>
#include <cmath>

namespace fengyin {
namespace {
constexpr juce::int64 cacheBudget=1024LL*1024*1024,singleBudget=512LL*1024*1024;
class PrepareJob final:public juce::ThreadPoolJob {
public:
    PrepareJob(juce::File tool,juce::File input,juce::File output,VideoPlaybackCache::Completion done)
        :ThreadPoolJob("Prepare idle video"),ffmpeg(std::move(tool)),source(std::move(input)),
         destination(std::move(output)),completion(std::move(done)){}
    JobStatus runJob() override {
        juce::Logger::writeToLog("Video cache: idle preparation started");
        struct CacheLock {
            juce::InterProcessLock lock{"FengYin-video-cache-v1"};
            const bool acquired=lock.enter(0);
            ~CacheLock(){if(acquired)lock.exit();}
        } guard;
        if(!guard.acquired){juce::Logger::writeToLog("Video cache: another process owns preparation");return finish({});}
        const auto probe=ffmpeg.getSiblingFile(
           #if JUCE_WINDOWS
            "ffprobe.exe"
           #else
            "ffprobe"
           #endif
        );
        // Never guess metadata or silently use an unverified/truncated proxy.
        if(!probe.existsAsFile()){juce::Logger::writeToLog("Video cache: ffprobe unavailable; use original");return finish({});}
        const auto inputInfo=info(probe,source);
        if(shouldExit())return jobHasFinished;
        if(!VideoPlaybackCache::worthwhile(inputInfo)){
            juce::Logger::writeToLog("Video cache: original retained; size="+juce::String(inputInfo.width)+"x"+juce::String(inputInfo.height)
                +" fps="+juce::String(inputInfo.fps)+" duration="+juce::String(inputInfo.duration));return finish({});
        }
        if(destination.existsAsFile()) {
            const auto cached=info(probe,destination);
            if(valid(cached,inputInfo)&&destination.getSize()<=singleBudget){
                juce::Logger::writeToLog("Video cache: verified cache hit");
                destination.setLastModificationTime(juce::Time::getCurrentTime());return finish(destination);
            }
            destination.deleteFile();
        }
        if(shouldExit())return jobHasFinished;
        if(!destination.getParentDirectory().createDirectory().wasOk())return finish({});
        // Limit only files owned by this version of the cache, never source
        // videos, audio-cache, recordings, or arbitrary neighbouring files.
        auto files=destination.getParentDirectory().findChildFiles(juce::File::findFiles,false,"v1-*.mp4");
        // An interrupted/crashed earlier run may leave a partial. The global
        // cache lock guarantees it cannot belong to a live preparation job.
        for(int i=files.size();--i>=0;) {
            const auto name=files[i].getFileNameWithoutExtension();
            if(name.length()==32 && name.endsWith(".part") && name.substring(3,27).containsOnly("0123456789abcdef")) {
                if(files[i].deleteFile())files.remove(i);
            }
        }
        std::sort(files.begin(),files.end(),[](const auto& a,const auto& b){return a.getLastModificationTime()<b.getLastModificationTime();});
        juce::int64 used=0;for(const auto& file:files)used+=file.getSize();
        const auto reservation=juce::jmin(singleBudget,static_cast<juce::int64>(inputInfo.duration*1000000.0)+1048576);
        if(destination.getParentDirectory().getBytesFreeOnVolume()<reservation+64*1024*1024)return finish({});
        for(const auto& file:files) {
            if(used+reservation<=cacheBudget)break;
            const auto name=file.getFileNameWithoutExtension();
            if(name.length()!=27||!name.substring(3).containsOnly("0123456789abcdef"))continue;
            const auto size=file.getSize();if(file.deleteFile())used-=size;
        }
        if(used+reservation>cacheBudget || shouldExit())return finish({});
        const auto partial=destination.getSiblingFile(destination.getFileNameWithoutExtension()+".part.mp4");
        partial.deleteFile();
        bool encoded=false;
        // Both paths are bounded single-thread transcodes. Hardware support is
        // machine-specific; failure must not affect normal video playback.
        for(const bool hardware:{true,false}) {
            if(shouldExit())break;
            const auto result=run(VideoPlaybackCache::encodeArguments(ffmpeg,source,partial,hardware),120000);
            if(result.first && !shouldExit() && partial.getSize()>64 && partial.getSize()<=singleBudget
                && valid(info(probe,partial),inputInfo)) {encoded=true;break;}
            if(!shouldExit())juce::Logger::writeToLog("Video cache encoder attempt failed: "+result.second.substring(0,256));
            partial.deleteFile();
        }
        if(shouldExit()){partial.deleteFile();return jobHasFinished;}
        encoded=encoded && used+partial.getSize()<=cacheBudget;
        if(encoded && partial.moveFileTo(destination))return finish(destination);
        partial.deleteFile();return finish({});
    }
private:
    std::pair<bool,juce::String> run(const juce::StringArray& args,int timeout) {
        juce::ChildProcess process;
        if(shouldExit()||!process.start(args,juce::ChildProcess::wantStdOut|juce::ChildProcess::wantStdErr))return {};
        const auto started=juce::Time::getMillisecondCounterHiRes();
        // ffprobe JSON and -loglevel error output are small; no per-frame pipe.
        while(process.isRunning()&&!shouldExit()&&juce::Time::getMillisecondCounterHiRes()-started<timeout)
            process.waitForProcessToFinish(50);
        if(process.isRunning()){process.kill();return {};}
        const auto text=process.readAllProcessOutput();return {process.getExitCode()==0,text};
    }
    VideoPlaybackCache::Info info(const juce::File& probe,const juce::File& file) {
        const auto result=run({probe.getFullPathName(),"-v","error","-select_streams","v:0",
            "-show_entries","stream=codec_name,width,height,avg_frame_rate,pix_fmt,color_transfer,color_primaries:format=duration","-of","json",file.getFullPathName()},10000);
        if(!result.first && !shouldExit())juce::Logger::writeToLog("Video cache probe failed: "+result.second.substring(0,256));
        return result.first?VideoPlaybackCache::readInfo(juce::JSON::parse(result.second)):VideoPlaybackCache::Info{};
    }
    static bool valid(const VideoPlaybackCache::Info& out,const VideoPlaybackCache::Info& in) {
        return out.codec=="h264" && out.width>0 && out.height>0 && out.width<=1920 && out.height<=1080
            && out.fps>0 && out.fps<=24.01 && std::abs(out.duration-in.duration)<.15;
    }
    JobStatus finish(const juce::File& file) {
        if(shouldExit())return jobHasFinished;
        auto done=std::move(completion);
        juce::MessageManager::callAsync([callback=std::move(done),file]{if(callback)callback(file);});
        return jobHasFinished;
    }
    juce::File ffmpeg,source,destination;
    VideoPlaybackCache::Completion completion;
};
}

VideoPlaybackCache::~VideoPlaybackCache(){pool.removeAllJobs(true,2000);}
void VideoPlaybackCache::interrupt(){pool.removeAllJobs(true,0);}
bool VideoPlaybackCache::prepare(const juce::File& ffmpeg,const juce::File& source,Completion done) {
    if(busy()||!ffmpeg.existsAsFile()||!source.existsAsFile())return false;
    const auto root=cacheDirectory==juce::File()?juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("FengYin").getChildFile("video-cache"):cacheDirectory;
    pool.addJob(new PrepareJob(ffmpeg,source,cacheFileFor(source,root),std::move(done)),true);return true;
}
juce::File VideoPlaybackCache::cacheFileFor(const juce::File& source,const juce::File& directory) {
    const auto identity=source.getFullPathName()+"|"+juce::String(source.getSize())+"|"
        +juce::String(source.getLastModificationTime().toMilliseconds())+"|1080p24-h264-v1";
    const auto hash=juce::SHA256(identity.toRawUTF8(),static_cast<size_t>(identity.getNumBytesAsUTF8())).toHexString().substring(0,24);
    return directory.getChildFile("v1-"+hash+".mp4");
}
VideoPlaybackCache::Info VideoPlaybackCache::readInfo(const juce::var& json) {
    Info result;const auto streams=json.getProperty("streams",juce::var());
    if(!streams.isArray()||streams.size()==0)return result;
    const auto stream=streams[0];result.width=static_cast<int>(stream.getProperty("width",0));
    result.height=static_cast<int>(stream.getProperty("height",0));result.codec=stream.getProperty("codec_name","").toString();
    const auto transfer=stream.getProperty("color_transfer","").toString();
    const auto primaries=stream.getProperty("color_primaries","").toString();
    const auto pixels=stream.getProperty("pix_fmt","").toString();
    // No unverified HDR/wide-gamut tone mapping or high-bit-depth conversion.
    const juce::StringArray safeTransfers{"","unknown","bt709","smpte170m","smpte240m","gamma22","gamma28","iec61966-2-1"};
    const juce::StringArray safePrimaries{"","unknown","bt709","bt470bg","bt470m","smpte170m","smpte240m"};
    result.standardDynamicRange=safeTransfers.contains(transfer)&&safePrimaries.contains(primaries)
        && !pixels.contains("10")&&!pixels.contains("12")&&!pixels.contains("16");
    const auto rate=juce::StringArray::fromTokens(stream.getProperty("avg_frame_rate","").toString(),"/","");
    if(rate.size()==2 && rate[1].getDoubleValue()>0)result.fps=rate[0].getDoubleValue()/rate[1].getDoubleValue();
    result.duration=json.getProperty("format",juce::var()).getProperty("duration",0).toString().getDoubleValue();
    return result;
}
bool VideoPlaybackCache::worthwhile(const Info& info) {
    // Avoid unbounded preparation of films/streams or guessing malformed media.
    return info.standardDynamicRange && info.width>0 && info.height>0 && info.fps>0 && std::isfinite(info.duration)
        && info.duration>.2 && info.duration<=500 && (info.width>1920||info.height>1080||info.fps>24.1||info.codec!="h264");
}
juce::StringArray VideoPlaybackCache::encodeArguments(const juce::File& ffmpeg,const juce::File& source,
                                                       const juce::File& destination,bool hardware) {
    juce::StringArray args{ffmpeg.getFullPathName(),"-hide_banner","-loglevel","error","-nostdin","-y","-threads","1",
        "-i",source.getFullPathName(),"-map","0:v:0","-an","-sn","-dn","-filter_threads","1","-vf",
        "fps=fps='min(source_fps,24)',scale=w='min(iw,1920)':h='min(ih,1080)':force_original_aspect_ratio=decrease:force_divisible_by=2",
        "-c:v",
       #if JUCE_WINDOWS
        "h264_mf","-hw_encoding",hardware?"1":"0",
       #elif JUCE_MAC
        "h264_videotoolbox","-allow_sw",hardware?"0":"1",
       #else
        "libx264","-preset","veryfast",
       #endif
        "-threads","1","-pix_fmt","yuv420p","-b:v","8M","-g","48","-movflags","+faststart",
        "-fs",juce::String(singleBudget),destination.getFullPathName()};
    juce::ignoreUnused(hardware);return args;
}
}
