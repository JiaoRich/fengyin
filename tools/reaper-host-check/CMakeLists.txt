cmake_minimum_required(VERSION 3.22)
project(FengYinIsolatedHostCheck VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(FENGYIN_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "Unmodified FengYin source root")
if(MSVC)
 set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
 add_compile_options(/utf-8 /UNDEBUG)
else()
 add_compile_options(-UNDEBUG)
endif()
include(FetchContent)
FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 9.0.2 GIT_SHALLOW TRUE
 PATCH_COMMAND ${CMAKE_COMMAND} -DJUCE_SOURCE_DIR=<SOURCE_DIR> -P "${FENGYIN_ROOT}/scripts/patch-juce-wasapi-raw.cmake")
FetchContent_MakeAvailable(JUCE)
add_executable(FengYinHostCheck main.cpp
 "${FENGYIN_ROOT}/native/Source/PluginHostEngine.cpp"
 "${FENGYIN_ROOT}/native/Source/SwamExpressionCurve.cpp"
 "${FENGYIN_ROOT}/native/Source/RecordingService.cpp"
 "${FENGYIN_ROOT}/native/Source/AccompanimentAudioService.cpp"
 "${FENGYIN_ROOT}/native/Source/MasterOutputService.cpp")
target_include_directories(FengYinHostCheck PRIVATE "${FENGYIN_ROOT}/native/Source")
target_compile_definitions(FengYinHostCheck PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(FengYinHostCheck PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
if(WIN32)
 target_link_libraries(FengYinHostCheck PRIVATE shell32)
endif()
enable_testing()
add_test(NAME isolated_render_self_test COMMAND FengYinHostCheck --self-test "${CMAKE_CURRENT_BINARY_DIR}/self-test-results")
set_tests_properties(isolated_render_self_test PROPERTIES TIMEOUT 60)
