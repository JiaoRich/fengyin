cmake_minimum_required(VERSION 3.22)
project(FengYin VERSION 1.1.40 LANGUAGES C CXX)
option(FENGYIN_ARCHIVED_VIDEO_RECORDING "Build archived video-recording diagnostics, never for release" OFF)
add_compile_definitions(FENGYIN_RELEASE_VERSION="${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Windows 安装包使用静态 MSVC 运行库，避免用户额外安装 VC++ 运行环境。
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/utf-8)
endif()

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.2
    GIT_SHALLOW TRUE
    PATCH_COMMAND ${CMAKE_COMMAND}
        -DJUCE_SOURCE_DIR=<SOURCE_DIR>
        -P ${CMAKE_SOURCE_DIR}/scripts/patch-juce-wasapi-raw.cmake
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(FengYin
    PRODUCT_NAME "FengYin"
    COMPANY_NAME "FengYin"
    BUNDLE_ID "cn.fengyin.windstation"
    ICON_BIG "${CMAKE_SOURCE_DIR}/assets/fengyin-app-icon.png"
    NEEDS_WEBVIEW2 TRUE
)

execute_process(COMMAND git rev-parse --short=12 HEAD
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_VARIABLE FENGYIN_BUILD_REVISION OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
if(NOT FENGYIN_BUILD_REVISION)
    set(FENGYIN_BUILD_REVISION "source-archive")
endif()
target_compile_definitions(FengYin PRIVATE FENGYIN_BUILD_REVISION="${FENGYIN_BUILD_REVISION}")

file(GLOB FENGYIN_VOICE_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/voice/*.wav")
juce_add_binary_data(FengYinWebAssets SOURCES
    ${FENGYIN_VOICE_FILES}
    prototype/voice-catalog.js
    prototype/voice-guide.js
    prototype/index.html
    prototype/styles.css
    prototype/app.js
    assets/fengyin-app-icon.png
    assets/tone-packages/soprano-kenny.fytonepack
    assets/tone-packages/soprano-pop.fytonepack
    assets/tone-packages/alto-jazz.fytonepack
    assets/tone-packages/alto-deep.fytonepack
    assets/tone-packages/alto-pop.fytonepack
    assets/tone-packages/tone137-alto-silky.fytonepack
    assets/tone-packages/tenor-mellow.fytonepack
    assets/tone-packages/tenor-warm.fytonepack
    assets/tone-packages/tenor-air-pocket.fytonepack
    assets/tone-packages/tone137-tenor-silky.fytonepack
    assets/tone-packages/trumpet.fytonepack
    assets/tone-packages/tone137-trumpet-2.fytonepack
    assets/tone-packages/piccolo-trumpet.fytonepack
    assets/tone-packages/alto-trombone.fytonepack
    assets/tone-packages/bass-trombone.fytonepack
    assets/tone-packages/flute.fytonepack
    assets/tone-packages/piccolo.fytonepack
    assets/tone-packages/clarinet.fytonepack
    assets/tone-packages/tone137-kong-clarinet-2.fytonepack
    assets/tone-packages/oboe.fytonepack
    assets/tone-packages/tone137-kong-oboe-2.fytonepack
    assets/tone-packages/violin-soft.fytonepack
    assets/tone-packages/tone137-kong-violin-2.fytonepack
    assets/tone-packages/cello.fytonepack
    assets/tone-packages/folk-erhu.fytonepack
    assets/tone-packages/tone137-kong-erhu-new-2.fytonepack
    assets/tone-packages/tone137-kong-hulusi-straight.fytonepack
    assets/tone-packages/tone137-kong-hulusi-vibrato.fytonepack
    assets/tone-packages/folk-liuqin.fytonepack
    assets/tone-packages/tone137-kong-qudi-straight.fytonepack
    assets/tone-packages/folk-qudi.fytonepack
    assets/tone-packages/tone137-kong-xun.fytonepack
    assets/tone-packages/tone137-kong-suona-vibrato.fytonepack
    assets/tone-packages/folk-suona.fytonepack
    assets/tone-packages/folk-sanxian.fytonepack
    assets/tone-packages/folk-pipa.fytonepack
    assets/tone-packages/tone137-kong-nanxiao.fytonepack
    assets/tone-packages/tone137-kong-dongxiao.fytonepack
    assets/tone-packages/folk-bangdi.fytonepack
    assets/tone-packages/folk-jinghu.fytonepack
    assets/tone-packages/kontakt-kensa.fytonepack
    assets/tone-packages/kong/erhu.kam
    assets/tone-packages/kong/guzheng.kam
    assets/tone-packages/kong/hulusi.kam
    assets/tone-packages/kong/liuqin.kam
    assets/tone-packages/kong/matouqin.kam
    assets/tone-packages/kong/qudi.kam
    assets/tone-packages/kong/xun.kam
    assets/tone-packages/kong/suona.kam
    assets/tone-packages/kong/sanxian.kam
    assets/tone-packages/kong/pipa.kam
    assets/tone-packages/kong/sheng.kam
    assets/tone-packages/kong/nanxiao.kam
    assets/tone-packages/kong/release-states.xml
    assets/instruments/instrument_soprano_sax.png
    assets/instruments/instrument_alto_sax.png
    assets/instruments/instrument_tenor_sax.png
    assets/instruments/instrument_baritone_sax.png
    assets/instruments/instrument_flugelhorn.png
    assets/instruments/instrument_trumpet.png
    assets/instruments/instrument_piccolo_trumpet.png
    assets/instruments/instrument_tenor_trombone.png
    assets/instruments/instrument_bass_trombone.png
    assets/instruments/instrument_tuba.png
    assets/instruments/instrument_euphonium.png
    assets/instruments/instrument_horn.png
    assets/instruments/instrument_piccolo.png
    assets/instruments/instrument_flute.png
    assets/instruments/instrument_alto_flute.png
    assets/instruments/instrument_bass_flute.png
    assets/instruments/instrument_clarinet.png
    assets/instruments/instrument_bass_clarinet.png
    assets/instruments/instrument_oboe.png
    assets/instruments/instrument_english_horn.png
    assets/instruments/instrument_bassoon.png
    assets/instruments/instrument_contrabassoon.png
    assets/instruments/instrument_violin.png
    assets/instruments/instrument_viola.png
    assets/instruments/instrument_cello.png
    assets/instruments/instrument_double_bass.png
    assets/instruments/instrument_kong_erhu.png
    assets/instruments/instrument_kong_guzheng.png
    assets/instruments/instrument_kong_dizi.png
    assets/instruments/instrument_kong_pipa.png
    assets/instruments/instrument_kong_suona.png
    assets/instruments/instrument_kong_hulusi.png
    assets/instruments/instrument_kong_yangqin.png
    assets/instruments/instrument_kong_guqin.png
    assets/instruments/instrument_kong_sheng.png
    assets/instruments/instrument_kong_ruan.png
    assets/instruments/instrument_kong_xun.png
    assets/instruments/instrument_kong_liuqin.png
    assets/instruments/instrument_kong_sanxian.png
    assets/instruments/instrument_kong_xiao.png
    assets/instruments/instrument_kong_banhu.png
)

target_sources(FengYin PRIVATE
    native/Source/VlcPlaybackEngine.cpp
    native/Source/VlcVideoSurface.cpp
    native/Source/Main.cpp
    native/Source/MainComponent.cpp
    native/Source/MainComponent.h
    native/Source/WindowRecordingService.cpp
    native/Source/WindowRecordingService.h
    native/Source/BluetoothMidiService.cpp
    native/Source/BluetoothMidiService.h
    native/Source/MidiInputService.cpp
    native/Source/MidiInputService.h
    native/Source/BreathMapper.h
    native/Source/DeviceProfile.h
    native/Source/TechniqueAdvisor.h
    native/Source/IntelligentTechniqueProcessor.h
    native/Source/ControllerDetector.h
    native/Source/MidiPerformanceSink.h
    native/Source/PitchKey.h
    native/Source/TestSynthEngine.cpp
    native/Source/TestSynthEngine.h
    native/Source/PluginCatalogService.cpp
    native/Source/PluginCatalogService.h
    native/Source/PluginHostEngine.cpp
    native/Source/PluginHostEngine.h
    native/Source/RealtimeMidiQueue.h
    native/Source/BleMidiInputDecoder.h
    native/Source/SwamTechniqueParameterPolicy.h
    native/Source/SwamPluginClassifier.h
    native/Source/KongInstrumentCatalog.h
    native/Source/ContainerPluginAdapter.h
    native/Source/SoundPresetStore.cpp
    native/Source/SoundPresetStore.h
    native/Source/VideoPlayerPanel.cpp
    native/Source/VideoPlayerPanel.h
    native/Source/LicenseService.cpp
    native/Source/LicenseService.h
    native/Source/RecordingService.cpp
    native/Source/RecordingService.h
    native/Source/AccompanimentAudioService.cpp
    native/Source/AccompanimentAudioService.h
    native/Source/VideoAudioExtractor.cpp
    native/Source/VideoAudioExtractor.h
    native/Source/VideoPlaybackCache.cpp
    native/Source/VideoPlaybackCache.h
    native/Source/MasterOutputService.h
    native/Source/MasterOutputService.cpp
    native/Source/ToneStyleCatalog.h
    native/Source/PublishedToneCatalog.h
    native/Source/SwamToneProfile.h
    native/Source/SwamExpressionCurve.cpp
    native/Source/SwamExpressionCurve.h
    native/Source/ToneParameterValue.h
    native/Source/KongLibraryLocator.h
    native/Source/KongProjectFile.h
    native/Source/AudioDeviceService.cpp
    native/Source/AudioDeviceService.h
    native/Source/WindowsLowLatencyOptimizer.cpp
    native/Source/WindowsLowLatencyOptimizer.h
    audio-engine/client/FengYinEngineAudioIODevice.cpp
    audio-engine/client/FengYinEngineAudioIODevice.h
    audio-engine/client/AudioEngineProcessController.cpp
    audio-engine/client/AudioEngineProcessController.h
    audio-engine/windows/NamedSharedAudioRegion.cpp
    audio-engine/windows/NamedSharedAudioRegion.h
    audio-engine/common/FengYinAudioProtocol.h
)

target_include_directories(FengYin PRIVATE
    native/Source
    audio-engine/client
    audio-engine/common
    audio-engine/windows
)

if(WIN32)
    set(FENGYIN_VLC_ROOT "${CMAKE_SOURCE_DIR}/third_party/vlc" CACHE PATH "Verified private LibVLC runtime")
    if(NOT EXISTS "${FENGYIN_VLC_ROOT}/sdk/include/vlc/vlc.h")
        message(FATAL_ERROR "Run scripts/prepare_vlc.py before building Windows")
    endif()
    target_include_directories(FengYin PRIVATE "${FENGYIN_VLC_ROOT}/sdk/include")
endif()

target_compile_definitions(FengYin PRIVATE
    JUCE_APPLICATION_NAME_STRING="FengYin"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=1
    FENGYIN_LOCK_WEBVIEW_ZOOM=1
    FENGYIN_TRACE_AUDIO_LOCKS=1 FENGYIN_RETAIN_AUDIO_SETUP=1
    JUCE_PLUGINHOST_VST3=1
)

target_link_libraries(FengYin PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_video
    juce::juce_cryptography
    juce::juce_dsp
    juce::juce_gui_extra
    FengYinWebAssets
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# 仅供销售者使用的激活码工具。它不会安装到客户电脑，也不会默认参与全量构建。
juce_add_gui_app(FengYinActivator
    PRODUCT_NAME "风吟激活码工具"
    COMPANY_NAME "FengYin"
    BUNDLE_ID "cn.fengyin.activator"
    ICON_BIG "${CMAKE_SOURCE_DIR}/assets/fengyin-app-icon.png"
)
set_target_properties(FengYinActivator PROPERTIES EXCLUDE_FROM_ALL TRUE)
target_sources(FengYinActivator PRIVATE
    tools/activator/Main.cpp
    tools/activator/ActivationRecordStore.cpp
    tools/activator/ActivationRecordStore.h
    native/Source/LicenseService.cpp
    native/Source/LicenseService.h
)
target_include_directories(FengYinActivator PRIVATE native/Source tools/activator)
target_compile_definitions(FengYinActivator PRIVATE
    JUCE_APPLICATION_NAME_STRING="FengYinActivator"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0
)
target_link_libraries(FengYinActivator PRIVATE
    juce::juce_cryptography
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

# 店主自动化使用的命令行生成器，同样不会进入客户安装包。
add_executable(FengYinLicenseGenerator EXCLUDE_FROM_ALL
    tools/license_generator.cpp
    native/Source/LicenseService.cpp
)
target_include_directories(FengYinLicenseGenerator PRIVATE native/Source)
target_link_libraries(FengYinLicenseGenerator PRIVATE juce::juce_cryptography)

if(WIN32)
    add_subdirectory(native/Recorder)
    # GPU recording remains a separate process, never in the real-time graph.
    add_executable(FengYinScreenCapture native/ScreenCapture/main.cpp)
    target_compile_definitions(FengYinScreenCapture PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    target_compile_options(FengYinScreenCapture PRIVATE /utf-8 /EHsc)
    target_link_libraries(FengYinScreenCapture PRIVATE windowsapp d3d11 dxgi dxguid mfplat mfreadwrite mfuuid evr ole32 uuid avrt)
    install(TARGETS FengYinScreenCapture RUNTIME DESTINATION .)
    target_sources(FengYin PRIVATE native/Source/WindowsCompatibility.manifest)
    target_link_libraries(FengYin PRIVATE setupapi advapi32 shell32 avrt dwmapi windowsapp psapi)
    if(FENGYIN_ARCHIVED_VIDEO_RECORDING)
    add_executable(FengYinScreenRecorder WIN32 native/ScreenRecorder/main.cpp)
    target_compile_definitions(FengYinScreenRecorder PRIVATE WINVER=0x0A00 _WIN32_WINNT=0x0A00)
    target_compile_options(FengYinScreenRecorder PRIVATE /utf-8)
    target_link_libraries(FengYinScreenRecorder PRIVATE windowsapp d3d11 dxgi dwmapi mfplat mfreadwrite mfuuid ole32 shell32 avrt)
    install(TARGETS FengYinScreenRecorder RUNTIME DESTINATION .)
    endif()
    target_compile_definitions(FengYin PRIVATE
        # ASIO 只供用户主动选择 KoordASIO 的 WASAPI 共享模式。默认和
        # 自动优化仍只使用 Windows 共享输出，不会打开独占驱动。
        JUCE_ASIO=1
        JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )

    add_executable(FengYinAudioEngine WIN32
        audio-engine/windows/AudioEngineMain.cpp
        audio-engine/windows/DefaultEndpointRouter.cpp
        audio-engine/windows/DefaultEndpointRouter.h
        audio-engine/windows/NamedSharedAudioRegion.cpp
        audio-engine/windows/NamedSharedAudioRegion.h
        audio-engine/windows/Asio4AllOutput.cpp
        audio-engine/windows/Asio4AllOutput.h
        audio-engine/windows/PhysicalOutputSelector.cpp
        audio-engine/windows/PhysicalOutputSelector.h
        audio-engine/windows/WasapiLoopbackInput.cpp
        audio-engine/windows/WasapiLoopbackInput.h
        audio-engine/service/AudioEngineCore.cpp
        audio-engine/service/AudioEngineCore.h
        audio-engine/common/FengYinAudioProtocol.h
    )
    target_include_directories(FengYinAudioEngine PRIVATE
        audio-engine/common
        audio-engine/service
        audio-engine/windows
    )
    target_compile_definitions(FengYinAudioEngine PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
        JUCE_ASIO=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
        FENGYIN_ASIO_RENDER_ONLY=1 JUCE_ASIO_DEBUGGING=1)
    target_link_libraries(FengYinAudioEngine PRIVATE ole32 avrt shell32 propsys juce::juce_audio_devices)

    add_executable(FengYinAudioWatchdog WIN32
        audio-engine/windows/AudioWatchdogMain.cpp
        audio-engine/windows/DefaultEndpointRouter.cpp
        audio-engine/windows/DefaultEndpointRouter.h
    )
    target_include_directories(FengYinAudioWatchdog PRIVATE audio-engine/windows)
    target_compile_definitions(FengYinAudioWatchdog PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(FengYinAudioWatchdog PRIVATE ole32 shell32 propsys)

    add_executable(FengYinDriverSetup WIN32
        audio-engine/windows/DriverSetupMain.cpp
        audio-engine/windows/DefaultEndpointRouter.cpp
        audio-engine/windows/DefaultEndpointRouter.h
    )
    target_include_directories(FengYinDriverSetup PRIVATE
        audio-engine/common
        audio-engine/windows
    )
    target_compile_definitions(FengYinDriverSetup PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_link_libraries(FengYinDriverSetup PRIVATE setupapi newdev cfgmgr32 ole32 shell32 propsys)
endif()

install(TARGETS FengYin RUNTIME DESTINATION . BUNDLE DESTINATION .)
if(WIN32)
    install(TARGETS FengYinAudioEngine FengYinAudioWatchdog FengYinDriverSetup RUNTIME DESTINATION .)
endif()

# 正式发布时把审核过许可的 ffmpeg.exe 放到这个位置，安装时会自动携带。
set(FENGYIN_FFMPEG_EXE
    "${CMAKE_SOURCE_DIR}/third_party/ffmpeg/windows/ffmpeg.exe"
    CACHE FILEPATH
    "用于提取视频音轨的 Windows FFmpeg 可执行文件"
)
if(WIN32 AND EXISTS "${FENGYIN_FFMPEG_EXE}")
    install(FILES "${FENGYIN_FFMPEG_EXE}" DESTINATION .)
endif()

install(FILES
    "${CMAKE_SOURCE_DIR}/README.md"
    "${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md"
    DESTINATION .
)

include(CTest)
if(BUILD_TESTING)
    if(APPLE)
        add_executable(PrepareKongDualSlotPackages EXCLUDE_FROM_ALL tools/prepare-kong-dual-slot-packages.cpp native/Source/SoundPresetStore.cpp)
        target_include_directories(PrepareKongDualSlotPackages PRIVATE native/Source)
        target_compile_definitions(PrepareKongDualSlotPackages PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_PLUGINHOST_VST3=1)
        target_link_libraries(PrepareKongDualSlotPackages PRIVATE juce::juce_audio_processors juce::juce_cryptography)
    endif()
    add_executable(AuditKongPackages EXCLUDE_FROM_ALL tools/audit-kong-packages.cpp native/Source/SoundPresetStore.cpp)
    target_include_directories(AuditKongPackages PRIVATE native/Source)
    target_compile_definitions(AuditKongPackages PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(AuditKongPackages PRIVATE juce::juce_audio_processors juce::juce_cryptography)
    add_executable(FengYinKongDualSlotTests tests/test_kong_dual_slot_router.cpp)
    target_include_directories(FengYinKongDualSlotTests PRIVATE native/Source)
    target_link_libraries(FengYinKongDualSlotTests PRIVATE juce::juce_audio_basics)
    target_compile_options(FengYinKongDualSlotTests PRIVATE
        $<$<CXX_COMPILER_ID:MSVC>:/UNDEBUG>
        $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-UNDEBUG>)
    add_test(NAME kong_dual_slot_exclusive_routing COMMAND FengYinKongDualSlotTests)
    add_executable(FengYinExportWaveTests tests/test_export_wave.cpp)
    add_test(NAME recording_aligned_30min_wave COMMAND FengYinExportWaveTests)
    find_package(Threads REQUIRED)
    add_executable(FengYinPcmFrameQueueTests tests/test_pcm_frame_queue.cpp)
    target_link_libraries(FengYinPcmFrameQueueTests PRIVATE Threads::Threads)
    add_test(NAME recording_pcm_frame_queue COMMAND FengYinPcmFrameQueueTests)
    add_executable(FengYinRecordingStartupTests tests/test_recording_startup_guard.cpp)
    add_test(NAME recording_startup_phase_guard COMMAND FengYinRecordingStartupTests)
    add_executable(FengYinScreenCapturePolicyTests tests/test_screen_capture_policy.cpp)
    add_test(NAME gpu_capture_bounded_policy COMMAND FengYinScreenCapturePolicyTests)
    add_executable(FengYinRecorderPolicyTests tests/test_recorder_policy.cpp)
    add_test(NAME recorder_scope_and_backend COMMAND FengYinRecorderPolicyTests)
    if(WIN32)
        juce_add_console_app(FengYinVideoChooserTests)
        target_sources(FengYinVideoChooserTests PRIVATE tests/test_video_file_chooser.cpp)
        target_include_directories(FengYinVideoChooserTests PRIVATE native/Source)
        target_compile_definitions(FengYinVideoChooserTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
        target_link_libraries(FengYinVideoChooserTests PRIVATE juce::juce_gui_extra juce::juce_recommended_config_flags)
        add_test(NAME video_file_selector_lifecycle COMMAND FengYinVideoChooserTests)
        set_tests_properties(video_file_selector_lifecycle PROPERTIES TIMEOUT 30)
        add_executable(FengYinRecordingServiceTests tests/test_window_recording_service.cpp native/Source/WindowRecordingService.cpp)
        target_link_libraries(FengYinRecordingServiceTests PRIVATE juce::juce_gui_basics)
        set_target_properties(FengYinRecordingServiceTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/recording-service-tests")
        add_executable(FengYinFakeGpuHelper tests/fake_recording_helper.cpp)
        set_target_properties(FengYinFakeGpuHelper PROPERTIES OUTPUT_NAME FengYinScreenCapture RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/recording-service-tests")
        add_executable(FengYinFakeAudioHelper tests/fake_recording_helper.cpp)
        set_target_properties(FengYinFakeAudioHelper PROPERTIES OUTPUT_NAME FengYinRecorder RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/recording-service-tests")
        add_dependencies(FengYinRecordingServiceTests FengYinFakeGpuHelper FengYinFakeAudioHelper)
        add_test(NAME recording_service_lifecycle COMMAND FengYinRecordingServiceTests)
        add_test(NAME dxgi_surface_sample_contract COMMAND FengYinScreenCapture --surface-buffer-selftest)
        add_executable(FengYinVlcTests tests/test_vlc_playback.cpp native/Source/VlcPlaybackEngine.cpp)
        target_include_directories(FengYinVlcTests PRIVATE native/Source "${FENGYIN_VLC_ROOT}/sdk/include")
        target_compile_definitions(FengYinVlcTests PRIVATE FENGYIN_VLC_TEST_PCM=1)
        target_link_libraries(FengYinVlcTests PRIVATE user32 gdi32)
        if(MSVC)
            target_compile_options(FengYinVlcTests PRIVATE /utf-8)
        endif()
        # Fixture is generated by the mandatory preflight, before the main app.
        add_test(NAME vlc_real_playback COMMAND FengYinVlcTests "${FENGYIN_VLC_ROOT}" "${CMAKE_BINARY_DIR}/vlc-playback-fixture.mp4")
        set_tests_properties(vlc_real_playback PROPERTIES TIMEOUT 100)
        # Unlike browser mocks or a bare HWND test, exercise the actual
        # WebView2 + production VLC child-window combination before release.
        add_subdirectory(tools/vlc-webview-check vlc-webview-check)
        find_package(Python3 REQUIRED COMPONENTS Interpreter)
        add_test(NAME vlc_webview_interaction COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_SOURCE_DIR}/scripts/check_vlc_webview.py"
            "${CMAKE_BINARY_DIR}" "${CMAKE_BINARY_DIR}/vlc-playback-fixture.mp4")
        set_tests_properties(vlc_webview_interaction PROPERTIES TIMEOUT 1600)
    endif()
    add_executable(FengYinMusicTempoTests tests/test_music_tempo.cpp)
    target_include_directories(FengYinMusicTempoTests PRIVATE native/Source)
    add_test(NAME music_tempo COMMAND FengYinMusicTempoTests)
    # Tests must execute their checks (and assert expressions) in Release too.
    if(MSVC)
        add_compile_options(/UNDEBUG)
    else()
        add_compile_options(-UNDEBUG)
    endif()
    add_executable(FengYinVirtualEndpointTests tests/test_virtual_endpoint_choice.cpp)
    add_test(NAME virtual_endpoint_selection COMMAND FengYinVirtualEndpointTests)
    add_executable(FengYinPresetTests
        tests/test_sound_preset_store.cpp
        native/Source/SoundPresetStore.cpp
        native/Source/SoundPresetStore.h
    )
    target_include_directories(FengYinPresetTests PRIVATE native/Source)
    target_link_libraries(FengYinPresetTests PRIVATE
        juce::juce_data_structures
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
    add_test(NAME sound_preset_roundtrip COMMAND FengYinPresetTests)
    add_executable(FengYinStudioTests tests/test_tone_studio.cpp native/Source/SoundPresetStore.cpp)
    target_include_directories(FengYinStudioTests PRIVATE native/Source)
    target_compile_definitions(FengYinStudioTests PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 FENGYIN_TRACE_AUDIO_LOCKS=1 FENGYIN_RETAIN_AUDIO_SETUP=1)
    target_link_libraries(FengYinStudioTests PRIVATE juce::juce_audio_processors juce::juce_cryptography juce::juce_recommended_config_flags)
    add_test(NAME tone_studio_chain_package COMMAND FengYinStudioTests)

    add_executable(FengYinAudioEngineProtocolTests
        tests/test_audio_engine_protocol.cpp
    )
    target_include_directories(FengYinAudioEngineProtocolTests PRIVATE audio-engine/common)
    add_test(NAME audio_engine_protocol COMMAND FengYinAudioEngineProtocolTests)

    add_executable(FengYinAudioEngineCoreTests
        tests/test_audio_engine_core.cpp
        audio-engine/service/AudioEngineCore.cpp
    )
    target_include_directories(FengYinAudioEngineCoreTests PRIVATE
        audio-engine/common
        audio-engine/service
    )
    add_test(NAME audio_engine_core COMMAND FengYinAudioEngineCoreTests)

    add_executable(FengYinLicenseTests tests/test_license_service.cpp native/Source/LicenseService.cpp)
    target_include_directories(FengYinLicenseTests PRIVATE native/Source)
    target_link_libraries(FengYinLicenseTests PRIVATE juce::juce_cryptography juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME license_validation COMMAND FengYinLicenseTests)

    add_executable(FengYinActivationRecordTests
        tests/test_activation_record_store.cpp
        tools/activator/ActivationRecordStore.cpp
        tools/activator/ActivationRecordStore.h
    )
    target_include_directories(FengYinActivationRecordTests PRIVATE tools/activator)
    target_link_libraries(FengYinActivationRecordTests PRIVATE juce::juce_core juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME activation_record_roundtrip COMMAND FengYinActivationRecordTests)

    add_executable(FengYinRecordingTests tests/test_recording_service.cpp native/Source/RecordingService.cpp native/Source/AccompanimentAudioService.cpp native/Source/VideoAudioExtractor.cpp native/Source/VideoPlaybackCache.cpp native/Source/MasterOutputService.cpp)
    target_include_directories(FengYinRecordingTests PRIVATE native/Source)
    target_link_libraries(FengYinRecordingTests PRIVATE juce::juce_audio_formats juce::juce_audio_utils juce::juce_cryptography juce::juce_events juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME wav_recording COMMAND FengYinRecordingTests)
    add_executable(FengYinBleDiscoveryQueueTests tests/test_ble_discovery_queue.cpp)
    add_test(NAME ble_discovery_queue COMMAND FengYinBleDiscoveryQueueTests)
    add_executable(FengYinAudioStageTimingTests tests/test_audio_stage_timing.cpp)
    add_test(NAME audio_stage_timing COMMAND FengYinAudioStageTimingTests)
    add_executable(FengYinCaptureFrameComposerTests tests/test_capture_frame_composer.cpp)
    add_test(NAME capture_frame_composer COMMAND FengYinCaptureFrameComposerTests)

    add_executable(FengYinSwamClassifierTests tests/test_swam_classifier.cpp)
    target_include_directories(FengYinSwamClassifierTests PRIVATE native/Source)
    add_test(NAME swam_classifier COMMAND FengYinSwamClassifierTests)

    add_executable(FengYinTechniqueAdvisorTests tests/test_technique_advisor.cpp)
    target_include_directories(FengYinTechniqueAdvisorTests PRIVATE native/Source)
    add_test(NAME technique_advisor COMMAND FengYinTechniqueAdvisorTests)

    add_executable(FengYinIntelligentTechniqueTests tests/test_intelligent_technique.cpp)
    target_include_directories(FengYinIntelligentTechniqueTests PRIVATE native/Source)
    add_test(NAME intelligent_technique COMMAND FengYinIntelligentTechniqueTests)

    add_executable(FengYinPitchKeyTests tests/test_pitch_key.cpp)
    target_include_directories(FengYinPitchKeyTests PRIVATE native/Source)
    add_test(NAME pitch_key_transpose COMMAND FengYinPitchKeyTests)

    add_executable(FengYinRealtimeMidiQueueTests tests/test_realtime_midi_queue.cpp)
    target_include_directories(FengYinRealtimeMidiQueueTests PRIVATE native/Source)
    add_test(NAME realtime_midi_queue COMMAND FengYinRealtimeMidiQueueTests)
    add_executable(FengYinCaptureAudioTimelineTests tests/test_capture_audio_timeline.cpp)
    add_test(NAME capture_audio_timeline COMMAND FengYinCaptureAudioTimelineTests)
    add_executable(FengYinAudioSetupRetentionTests tests/test_audio_setup_retention.cpp)
    target_compile_definitions(FengYinAudioSetupRetentionTests PRIVATE FENGYIN_RETAIN_AUDIO_SETUP=1)
    target_link_libraries(FengYinAudioSetupRetentionTests PRIVATE juce::juce_audio_devices juce::juce_recommended_config_flags)
    add_test(NAME audio_setup_retention COMMAND FengYinAudioSetupRetentionTests)

    add_executable(FengYinBleMidiDecoderTests tests/test_ble_midi_decoder.cpp)
    target_include_directories(FengYinBleMidiDecoderTests PRIVATE native/Source)
    add_test(NAME ble_midi_decoder COMMAND FengYinBleMidiDecoderTests)
    add_executable(FengYinBluetoothMidiTests tests/test_bluetooth_midi.cpp native/Source/BluetoothMidiService.cpp)
    target_include_directories(FengYinBluetoothMidiTests PRIVATE native/Source)
    target_link_libraries(FengYinBluetoothMidiTests PRIVATE juce::juce_audio_basics juce::juce_data_structures juce::juce_events juce::juce_recommended_config_flags)
    if(WIN32)
        target_link_libraries(FengYinBluetoothMidiTests PRIVATE windowsapp)
        target_compile_options(FengYinBluetoothMidiTests PRIVATE /utf-8)
    endif()
    add_test(NAME bluetooth_midi_lifecycle COMMAND FengYinBluetoothMidiTests)
    set_tests_properties(bluetooth_midi_lifecycle PROPERTIES TIMEOUT 20)
    add_executable(FengYinMidiOnsetTests tests/test_midi_onset.cpp native/Source/MidiInputService.cpp)
    target_include_directories(FengYinMidiOnsetTests PRIVATE native/Source)
    target_link_libraries(FengYinMidiOnsetTests PRIVATE juce::juce_audio_devices juce::juce_data_structures juce::juce_recommended_config_flags)
    add_test(NAME midi_phrase_onset COMMAND FengYinMidiOnsetTests)
    add_executable(FengYinVoicePromptTests tests/test_voice_prompt.cpp)
    target_include_directories(FengYinVoicePromptTests PRIVATE native/Source)
    target_link_libraries(FengYinVoicePromptTests PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
    add_test(NAME offline_voice_prompt COMMAND FengYinVoicePromptTests "${CMAKE_SOURCE_DIR}/assets/voice/V041.wav")
    add_test(NAME recording_prepare_audio_prompt COMMAND FengYinVoicePromptTests "${CMAKE_SOURCE_DIR}/assets/voice/REC_PREP_AUDIO.wav")
    add_test(NAME recording_prepare_screen_prompt COMMAND FengYinVoicePromptTests "${CMAKE_SOURCE_DIR}/assets/voice/REC_PREP_SCREEN.wav")
    add_test(NAME recording_begin_beep COMMAND FengYinVoicePromptTests "${CMAKE_SOURCE_DIR}/assets/voice/REC_BEGIN_BEEP.wav")
    add_test(NAME recording_saved_prompt COMMAND FengYinVoicePromptTests "${CMAKE_SOURCE_DIR}/assets/voice/REC_SAVED.wav")

    add_executable(FengYinMasterOutputGuardTests tests/test_master_output_guard.cpp native/Source/MasterOutputService.cpp)
    target_include_directories(FengYinMasterOutputGuardTests PRIVATE native/Source)
    target_link_libraries(FengYinMasterOutputGuardTests PRIVATE juce::juce_dsp juce::juce_recommended_config_flags)
    add_test(NAME master_output_guard COMMAND FengYinMasterOutputGuardTests)
    add_executable(FengYinToneStyleTests tests/test_tone_style_catalog.cpp native/Source/MasterOutputService.cpp)
    target_include_directories(FengYinToneStyleTests PRIVATE native/Source)
    target_link_libraries(FengYinToneStyleTests PRIVATE juce::juce_dsp juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME tone_style_catalog COMMAND FengYinToneStyleTests)

    add_executable(FengYinAirTests tests/test_air_processor.cpp)
    target_include_directories(FengYinAirTests PRIVATE native/Source)
    add_test(NAME air_processor COMMAND FengYinAirTests)

    add_executable(FengYinSwamExpressionCurveTests
        tests/test_swam_expression_curve.cpp
        native/Source/SwamExpressionCurve.cpp
    )
    target_include_directories(FengYinSwamExpressionCurveTests PRIVATE native/Source)
    target_compile_definitions(FengYinSwamExpressionCurveTests PRIVATE JUCE_PLUGINHOST_VST3=1)
    target_link_libraries(FengYinSwamExpressionCurveTests PRIVATE
        juce::juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
    add_test(NAME swam_expression_curve COMMAND FengYinSwamExpressionCurveTests)

    add_executable(FengYinToneDiagnosticTests tests/test_tone_diagnostic.cpp)
    target_include_directories(FengYinToneDiagnosticTests PRIVATE native/Source)
    target_link_libraries(FengYinToneDiagnosticTests PRIVATE juce::juce_audio_processors
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME tone_diagnostic_snapshot COMMAND FengYinToneDiagnosticTests)
    add_executable(FengYinSwamToneApplicationTests tests/test_swam_tone_application.cpp)
    target_include_directories(FengYinSwamToneApplicationTests PRIVATE native/Source)
    target_link_libraries(FengYinSwamToneApplicationTests PRIVATE juce::juce_audio_processors
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME swam_tone_application COMMAND FengYinSwamToneApplicationTests)

    add_executable(FengYinBendRangeTests tests/test_bend_range.cpp)
    target_include_directories(FengYinBendRangeTests PRIVATE native/Source)
    target_link_libraries(FengYinBendRangeTests PRIVATE juce::juce_audio_processors
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME bend_range_parameters COMMAND FengYinBendRangeTests)

    add_executable(FengYinPerformanceResetTests tests/test_performance_reset.cpp)
    target_include_directories(FengYinPerformanceResetTests PRIVATE native/Source)
    target_link_libraries(FengYinPerformanceResetTests PRIVATE juce::juce_audio_basics
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    add_test(NAME performance_reset_controllers COMMAND FengYinPerformanceResetTests)
    add_executable(FengYinCompactGlowTests tests/test_compact_glow.cpp)
    target_include_directories(FengYinCompactGlowTests PRIVATE native/Source)
    target_link_libraries(FengYinCompactGlowTests PRIVATE juce::juce_gui_basics
        juce::juce_recommended_config_flags)
    add_test(NAME compact_glow_rendering COMMAND FengYinCompactGlowTests)
    if(WIN32)
        target_sources(FengYinCompactGlowTests PRIVATE native/Source/WindowsCompatibility.manifest)
        target_compile_definitions(FengYinCompactGlowTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
        add_test(NAME compact_glow_windows_desktop COMMAND FengYinCompactGlowTests --desktop-smoke)
        set_tests_properties(compact_glow_windows_desktop PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 30)
    endif()

endif()
