cmake_minimum_required(VERSION 3.24)
project(FengYinVlcWebViewCheck VERSION 1.1.25 LANGUAGES C CXX)
if(NOT WIN32)
    message(FATAL_ERROR "Requires real Windows WebView2 and VLC HWNDs")
endif()
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(NOT TARGET juce::juce_gui_extra)
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.2 GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()
juce_add_console_app(FengYinVlcWebViewCheck NEEDS_WEBVIEW2 TRUE)
set_target_properties(FengYinVlcWebViewCheck PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIG>")
target_sources(FengYinVlcWebViewCheck PRIVATE main.cpp
    "${ROOT}/native/Source/VlcVideoSurface.cpp"
    "${ROOT}/native/Source/VlcPlaybackEngine.cpp"
    "${ROOT}/native/Source/WindowsCompatibility.manifest")
target_include_directories(FengYinVlcWebViewCheck PRIVATE "${ROOT}/native/Source" "${ROOT}/third_party/vlc/sdk/include")
target_compile_definitions(FengYinVlcWebViewCheck PRIVATE JUCE_WEB_BROWSER=1 JUCE_USE_CURL=0
    JUCE_USE_WIN_WEBVIEW2_WITH_STATIC_LINKING=1 JUCE_MODAL_LOOPS_PERMITTED=1 FENGYIN_VLC_TEST_PCM=1
    FENGYIN_DIAGNOSTIC_FRONTEND="${ROOT}/prototype/index.html")
target_compile_options(FengYinVlcWebViewCheck PRIVATE /utf-8 /EHsc)
target_link_libraries(FengYinVlcWebViewCheck PRIVATE juce::juce_gui_extra juce::juce_recommended_config_flags user32 gdi32 dbghelp)
# The production surface resolves VLC beside its own executable. The standalone
# workflow staged this runtime, but the full installer preflight did not. Keep
# the executable self-contained in both entry points, before any GUI case runs.
if(NOT DEFINED FENGYIN_VLC_ROOT)
    set(FENGYIN_VLC_ROOT "${ROOT}/third_party/vlc")
endif()
add_custom_command(TARGET FengYinVlcWebViewCheck POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${FENGYIN_VLC_ROOT}"
        "$<TARGET_FILE_DIR:FengYinVlcWebViewCheck>/components/VLC"
    VERBATIM)
