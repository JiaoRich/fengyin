#include "VlcPlaybackEngine.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iterator>
#include <stdexcept>
#if defined(FENGYIN_VLC_TEST_PCM)
#include <iostream>
#endif
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <vlc/vlc.h>
#endif

namespace fengyin
{
VlcPlaybackEngine::VlcPlaybackEngine(std::filesystem::path root, void* hwnd)
    : directory(std::move(root)), window(hwnd), worker([this] { run(); }) {}
VlcPlaybackEngine::~VlcPlaybackEngine()
{
    requestShutdown();
    if (worker.joinable()) worker.join();
}
void VlcPlaybackEngine::requestShutdown()
{
    { std::lock_guard lock(mutex); request.quitting = true; }
    wake.notify_one();
}
void VlcPlaybackEngine::load(std::string path, int generation)
{
    { std::lock_guard lock(mutex);
      request.path = std::move(path); request.generation = generation;
      request.playing = false; ++request.playRevision;
      request.seekRevision = 0; request.position = 0;
      const auto failure=stopped.load()?state.error:std::string{};
      state = {}; state.generation = generation; state.error=failure; }
    wake.notify_one();
}
void VlcPlaybackEngine::unload() { load({}, getState().generation + 1); }
void VlcPlaybackEngine::setPlaying(bool playing)
{
    { std::lock_guard lock(mutex); request.playing = playing; ++request.playRevision;
      if (playing) { state.finished=false; state.error.clear(); } }
    wake.notify_one();
}
void VlcPlaybackEngine::seek(double seconds, int revision)
{
    if (!std::isfinite(seconds)) return;
    { std::lock_guard lock(mutex);
      if (revision < request.seekRevision) return;
      request.position = std::max(0.0, seconds); request.seekRevision = revision; }
    wake.notify_one();
}
void VlcPlaybackEngine::setVolume(float volume)
{
    if (!std::isfinite(volume)) return;
    { std::lock_guard lock(mutex); request.volume = std::clamp(volume, 0.0f, 1.0f); }
    wake.notify_one();
}
VlcPlaybackEngine::State VlcPlaybackEngine::getState() const
{
    std::lock_guard lock(mutex); return state;
}

#if defined(_WIN32)
namespace
{
struct VlcApi
{
    HMODULE core = nullptr, library = nullptr;
#define FY_VLC_FUNCTIONS(X) \
 X(libvlc_new) X(libvlc_release) X(libvlc_media_new_path) X(libvlc_media_release) \
 X(libvlc_media_parse_with_options) X(libvlc_media_parse_stop) X(libvlc_media_get_duration) \
 X(libvlc_media_get_stats) X(libvlc_media_tracks_get) X(libvlc_media_tracks_release) \
 X(libvlc_media_player_new) X(libvlc_media_player_release) \
 X(libvlc_media_player_set_media) X(libvlc_media_player_set_hwnd) \
 X(libvlc_media_player_play) X(libvlc_media_player_set_pause) X(libvlc_media_player_stop) \
 X(libvlc_media_player_get_state) X(libvlc_media_player_get_time) X(libvlc_media_player_get_length) \
 X(libvlc_media_player_set_time) X(libvlc_audio_set_volume) \
 X(libvlc_video_set_mouse_input) X(libvlc_video_set_key_input) \
 X(libvlc_media_player_event_manager) X(libvlc_event_attach) X(libvlc_event_detach)
#if defined(FENGYIN_VLC_TEST_PCM)
    decltype(&::libvlc_audio_set_callbacks) libvlc_audio_set_callbacks=nullptr;
    decltype(&::libvlc_audio_set_format) libvlc_audio_set_format=nullptr;
    decltype(&::libvlc_clock) libvlc_clock=nullptr;
#endif
#define FY_DECLARE(name) decltype(&::name) name = nullptr;
    FY_VLC_FUNCTIONS(FY_DECLARE)
#undef FY_DECLARE
    explicit VlcApi(const std::filesystem::path& root)
    {
        // Absolute application-private directory; never resolve a user's VLC or
        // load DLLs from the working directory. Keep modules until VLC is gone.
        constexpr DWORD flags = LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS;
        core = LoadLibraryExW((root / L"libvlccore.dll").c_str(), nullptr, flags);
        library = LoadLibraryExW((root / L"libvlc.dll").c_str(), nullptr, flags);
        if (!core || !library) { close(); throw std::runtime_error("VLC 播放组件缺失，请重新安装风吟"); }
        try {
#define FY_RESOLVE(name) name = reinterpret_cast<decltype(name)>(GetProcAddress(library, #name)); \
            if (!name) throw std::runtime_error("VLC 播放组件版本不匹配，请重新安装风吟");
            FY_VLC_FUNCTIONS(FY_RESOLVE)
#if defined(FENGYIN_VLC_TEST_PCM)
            FY_RESOLVE(libvlc_audio_set_callbacks)
            FY_RESOLVE(libvlc_audio_set_format)
            FY_RESOLVE(libvlc_clock)
#endif
#undef FY_RESOLVE
        } catch (...) { close(); throw; }
    }
    void close() { if(library)FreeLibrary(library); if(core)FreeLibrary(core); library=nullptr;core=nullptr; }
    ~VlcApi() { close(); }
};
// VLC 3 input_event_changed emits EncounteredError, then INPUT_EVENT_DEAD
// replaces the observable player state with Ended. Polling every 100ms can
// miss that error entirely. Latch it on the event thread without calling VLC
// or the UI from the callback. Reset only after the old input has stopped.
struct VlcErrorEvents
{
    VlcApi& api;
    libvlc_event_manager_t* manager;
    std::atomic<bool> failed{false};
    bool attached=false;
    static void callback(const libvlc_event_t*, void* context) noexcept
    { static_cast<VlcErrorEvents*>(context)->failed.store(true); }
    VlcErrorEvents(VlcApi& functions, libvlc_media_player_t* player)
        : api(functions), manager(api.libvlc_media_player_event_manager(player))
    {
        attached=manager && api.libvlc_event_attach(manager,libvlc_MediaPlayerEncounteredError,callback,this)==0;
    }
    void detach() noexcept
    {
        if(attached)api.libvlc_event_detach(manager,libvlc_MediaPlayerEncounteredError,callback,this);
        attached=false;
    }
    ~VlcErrorEvents(){detach();}
};
}
#endif

void VlcPlaybackEngine::run()
{
#if defined(_WIN32)
    try
    {
        VlcApi api(directory);
        const char* options[] = {"--ignore-config", "--no-media-library", "--no-video-title-show",
                                 "--no-osd", "--no-interact", "--avcodec-hw=any"};
        auto* instance = api.libvlc_new(static_cast<int>(std::size(options)), options);
        if (!instance) throw std::runtime_error("无法初始化 VLC 播放组件");
        auto* player = api.libvlc_media_player_new(instance);
        if (!player) { api.libvlc_release(instance); throw std::runtime_error("无法创建 VLC 播放器"); }
        VlcErrorEvents errorEvents(api,player);
        if(!errorEvents.attached) {
            api.libvlc_media_player_release(player);api.libvlc_release(instance);
            throw std::runtime_error("无法监听 VLC 播放状态");
        }
        api.libvlc_media_player_set_hwnd(player, window);
        api.libvlc_video_set_mouse_input(player, 0);
        api.libvlc_video_set_key_input(player, 0);
#if defined(FENGYIN_VLC_TEST_PCM)
        // CI-only decoded PCM sink: runners need no speaker endpoint. Never
        // compiled into FengYin; production uses VLC's normal audio output.
        // VLC 3.0 amem.c supports S16N, not FL32. Respect its delivery timestamp
        // rather than draining decoded audio faster than real-time in CI.
        struct TestAudio {VlcPlaybackEngine* owner;decltype(api.libvlc_clock) clock;};
        TestAudio testAudio{this,api.libvlc_clock};
        api.libvlc_audio_set_format(player,"S16N",48000,2);
        api.libvlc_audio_set_callbacks(player,[](void* opaque,const void* samples,unsigned count,int64_t pts){
            auto& context=*static_cast<TestAudio*>(opaque);
            auto& engine=*context.owner;
            const auto delay=pts-context.clock();
            if(delay>0)Sleep(static_cast<DWORD>(std::min<int64_t>(delay/1000,500)));
            const auto* data=static_cast<const int16_t*>(samples);
            uint64_t nonzero=0;
            for(unsigned i=0;i<count*2;++i) if(std::abs(static_cast<int>(data[i]))>32)++nonzero;
            engine.testPcmSamples.fetch_add(count*2);engine.testPcmNonzero.fetch_add(nonzero);
        },nullptr,nullptr,nullptr,nullptr,&testAudio);
#endif
        libvlc_media_t* media = nullptr;
        Request applied;
        applied.generation = -1; applied.playRevision = -1;
        double pendingSeek = -1;
        bool started = false;
        bool terminalChecked = false;
        std::string playbackError;
        while (true)
        {
            Request next;
            { std::unique_lock lock(mutex); wake.wait_for(lock, std::chrono::milliseconds(100)); next=request; }
            if (next.quitting) break;
            State result;
            result.generation=next.generation; result.seekRevision=next.seekRevision;
            if (next.generation != applied.generation)
            {
                api.libvlc_media_player_stop(player);
                errorEvents.failed.store(false);
                api.libvlc_media_player_set_media(player, nullptr);
                if(media) { api.libvlc_media_parse_stop(media); api.libvlc_media_release(media); media=nullptr; }
                started=false; terminalChecked=false; pendingSeek=-1;playbackError.clear();
                if (!next.path.empty())
                {
                    // VLC 3 Windows vlc_path2uri rejects drive paths using '/'
                    // (ENOTSUP). CMake/test paths and imported paths may use it.
                    // Keep UTF-8 bytes intact; normalise separators, not encoding.
                    auto nativePath=next.path;
                    std::replace(nativePath.begin(),nativePath.end(),'/','\\');
                    media=api.libvlc_media_new_path(instance,nativePath.c_str());
                    if (media)
                    {
                        api.libvlc_media_player_set_media(player,media);
                        // Metadata only, no full-file extraction/transcoding and
                        // no playback to obtain a preview. Never fetch network art.
                        api.libvlc_media_parse_with_options(media,libvlc_media_parse_local,2500);
                    }
                }
                applied=next; applied.playRevision=-1; applied.seekRevision=-1; applied.volume=-1;
            }
            if (media)
            {
                result.ready=true;
                if(next.volume!=applied.volume)
                    api.libvlc_audio_set_volume(player,static_cast<int>(std::lround(next.volume*100)));
                if(next.seekRevision!=applied.seekRevision && next.seekRevision>0) pendingSeek=next.position;
                auto playbackState=api.libvlc_media_player_get_state(player);
                if(next.playRevision!=applied.playRevision)
                {
                    if(next.playing)
                    {
                        playbackError.clear();
                        if(playbackState==libvlc_Ended || playbackState==libvlc_Error)
                        { api.libvlc_media_player_stop(player); errorEvents.failed.store(false);
                          started=false; terminalChecked=false; if(pendingSeek<0)pendingSeek=0; }
                        if(!started) { started=api.libvlc_media_player_play(player)==0; }
                        else api.libvlc_media_player_set_pause(player,0);
                        if(!started) playbackError="视频无法播放，请检查文件是否完整";
                    }
                    else if(started) api.libvlc_media_player_set_pause(player,1);
                }
                playbackState=api.libvlc_media_player_get_state(player);
                if(!next.playing && playbackState==libvlc_Playing)
                    api.libvlc_media_player_set_pause(player,1);
                if(pendingSeek>=0 && (playbackState==libvlc_Playing || playbackState==libvlc_Paused))
                { api.libvlc_media_player_set_time(player,static_cast<int64_t>(pendingSeek*1000)); pendingSeek=-1; }
                result.playing=playbackState==libvlc_Playing || (next.playing && (playbackState==libvlc_Opening || playbackState==libvlc_Buffering));
                result.finished=playbackState==libvlc_Ended;
                if(playbackState==libvlc_Error || errorEvents.failed.load())
                    playbackError="VLC 无法播放这个文件，请重新选择视频";
                result.duration=std::max<int64_t>(0,api.libvlc_media_player_get_length(player))/1000.0;
                if(result.duration==0)result.duration=std::max<int64_t>(0,api.libvlc_media_get_duration(media))/1000.0;
                result.position=pendingSeek>=0 ? pendingSeek : std::max<int64_t>(0,api.libvlc_media_player_get_time(player))/1000.0;
                libvlc_media_stats_t stats{};
                if(api.libvlc_media_get_stats(media,&stats)) {
                    result.decodedVideo=stats.i_decoded_video; result.displayedVideo=stats.i_displayed_pictures;
                    result.lostVideo=stats.i_lost_pictures; result.decodedAudio=stats.i_decoded_audio;
                }
                // Some demuxers accept non-media input then return ordinary
                // EOF, without EncounteredError. Only inspect tracks at the
                // terminal state: missing metadata during opening is normal.
                // Valid audio-only, silent audio, video-only and EOF seeks
                // remain valid even if this playback decoded no samples.
                if(started && playbackState==libvlc_Ended && !terminalChecked)
                {
                    terminalChecked=true;
                    libvlc_media_track_t** tracks=nullptr;
                    const auto count=api.libvlc_media_tracks_get(media,&tracks);
                    bool audiovisual=false;
                    for(unsigned i=0;i<count;++i)
                        if(tracks[i] && (tracks[i]->i_type==libvlc_track_audio || tracks[i]->i_type==libvlc_track_video))
                            audiovisual=true;
                    api.libvlc_media_tracks_release(tracks,count);
#if defined(FENGYIN_VLC_TEST_PCM)
                    std::cout<<"VLC terminal generation="<<next.generation<<" tracks="<<count
                             <<" av="<<audiovisual<<" video="<<result.decodedVideo
                             <<" audio="<<result.decodedAudio<<" errorEvent="<<errorEvents.failed.load()<<"\n";
#endif
                    if(!audiovisual && result.decodedVideo==0 && result.decodedAudio==0)
                        playbackError="这个文件没有可播放的音视频内容，请重新选择";
                }
                result.error=playbackError;
                if(!result.error.empty()){result.ready=false;result.playing=false;result.finished=false;}
            }
            else if(!next.path.empty())result.error="无法打开这个视频文件";
            applied=next;
            { std::lock_guard lock(mutex);
              if(request.generation==result.generation && request.seekRevision==result.seekRevision
                 && request.playRevision==next.playRevision) state=std::move(result); }
        }
        api.libvlc_media_player_stop(player);
        errorEvents.detach();
        api.libvlc_media_player_release(player);
        if(media) { api.libvlc_media_parse_stop(media); api.libvlc_media_release(media); }
        api.libvlc_release(instance);
    }
    catch(const std::exception& e)
    {
        std::lock_guard lock(mutex); state.generation=request.generation; state.error=e.what();
    }
#else
    { std::lock_guard lock(mutex); state.error="当前平台未打包 VLC 播放组件"; }
#endif
    stopped.store(true);
}
}
