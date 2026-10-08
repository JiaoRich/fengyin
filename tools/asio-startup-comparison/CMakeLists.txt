cmake_minimum_required(VERSION 3.22)
project(FengYinAsioComparison LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.2 GIT_SHALLOW TRUE
    PATCH_COMMAND ${CMAKE_COMMAND} -DJUCE_SOURCE_DIR=<SOURCE_DIR> -P ${ROOT}/scripts/patch-juce-wasapi-raw.cmake)
FetchContent_MakeAvailable(JUCE)
add_executable(FengYinAsioComparison Main.cpp)
target_include_directories(FengYinAsioComparison PRIVATE ${ROOT}/audio-engine/windows)
target_compile_definitions(FengYinAsioComparison PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN
    JUCE_ASIO=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_MODAL_LOOPS_PERMITTED=1
    FENGYIN_ASIO_RENDER_ONLY=1 JUCE_ASIO_DEBUGGING=1)
target_compile_options(FengYinAsioComparison PRIVATE /utf-8 /Zi)
target_link_options(FengYinAsioComparison PRIVATE /DEBUG:FULL)
target_link_libraries(FengYinAsioComparison PRIVATE ole32 advapi32 shell32 uuid juce::juce_audio_devices)
