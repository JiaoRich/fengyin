# FluidSynth is a replaceable shared library. No device driver, daemon, CLI,
# downloaded SF2 data or third-party instrument licence is installed.
function(fengyin_add_fluid)
    set(BUILD_SHARED_LIBS ON)
    set(BUILD_TESTING OFF)
    set(osal cpp11 CACHE STRING "FluidSynth OS abstraction" FORCE)
    foreach(feature alsa aufile dbus ipv6 jack ladspa libsndfile midishare opensles oboe network oss dsound wasapi waveout winmidi sdl3 pulseaudio pipewire readline openmp native-dls signalsmith coreaudio coremidi framework systemd portaudio)
        set(enable-${feature} OFF CACHE BOOL "Disabled for FengYin's PCM-only SF2 component" FORCE)
    endforeach()
    FetchContent_Declare(FengYinFluid
        URL https://github.com/FluidSynth/fluidsynth/archive/refs/tags/v2.6.3.tar.gz
        URL_HASH SHA256=c51ebe04c8a445f4a7dd5a86e09a27b5c5af547756574edc52fc61c88ebe8427
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(FengYinFluid)
    set(FENGYIN_FLUID_SOURCE "${fengyinfluid_SOURCE_DIR}" PARENT_SCOPE)
    set(FENGYIN_FLUID_BINARY "${fengyinfluid_BINARY_DIR}" PARENT_SCOPE)
endfunction()
fengyin_add_fluid()

juce_add_plugin(FengYinSF2
    COMPANY_NAME FengYin BUNDLE_ID cn.fengyin.sf2
    PLUGIN_MANUFACTURER_CODE FyIn PLUGIN_CODE FyS2
    FORMATS VST3 PRODUCT_NAME "FengYin SF2"
    IS_SYNTH TRUE NEEDS_MIDI_INPUT TRUE NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE VST3_AUTO_MANIFEST FALSE)
target_sources(FengYinSF2 PRIVATE native/Sf2/Sf2Processor.cpp native/Sf2/Sf2Editor.cpp)
target_include_directories(FengYinSF2 PRIVATE native/Sf2 native/Source)
target_compile_definitions(FengYinSF2 PUBLIC JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0)
target_link_libraries(FengYinSF2 PRIVATE libfluidsynth juce::juce_audio_utils juce::juce_cryptography PUBLIC juce::juce_recommended_config_flags)

juce_add_console_app(FengYinSF2Probe PRODUCT_NAME FengYinSF2Probe)
target_sources(FengYinSF2Probe PRIVATE native/Sf2/Sf2Probe.cpp)
target_include_directories(FengYinSF2Probe PRIVATE native/Sf2)
target_compile_definitions(FengYinSF2Probe PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
target_link_libraries(FengYinSF2Probe PRIVATE libfluidsynth juce::juce_cryptography)
add_dependencies(FengYinSF2_VST3 FengYinSF2Probe)
if(WIN32)
    target_link_libraries(FengYinSF2 PRIVATE ole32 shell32 uuid comctl32)
    add_custom_command(TARGET FengYinSF2Probe POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:libfluidsynth> $<TARGET_FILE_DIR:FengYinSF2Probe>)
endif()
if(APPLE)
    set_target_properties(FengYinSF2_VST3 FengYinSF2Probe PROPERTIES BUILD_RPATH "@loader_path")
endif()
add_custom_command(TARGET FengYinSF2_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:FengYinSF2Probe> $<TARGET_FILE_DIR:FengYinSF2_VST3>)
# DLLs have no SONAME generator expression on Windows.
if(WIN32)
    # The Windows staging script also checks this dependency by its PE imports.
    add_custom_command(TARGET FengYinSF2_VST3 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:libfluidsynth> $<TARGET_FILE_DIR:FengYinSF2_VST3>)
endif()
if(APPLE)
    add_custom_command(TARGET FengYinSF2_VST3 POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:libfluidsynth> $<TARGET_FILE_DIR:FengYinSF2_VST3>/$<TARGET_SONAME_FILE_NAME:libfluidsynth>)
    add_custom_command(TARGET FengYinSF2_VST3 POST_BUILD COMMAND /usr/bin/codesign --force --deep --sign -
        "$<GENEX_EVAL:$<TARGET_PROPERTY:FengYinSF2_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>")
endif()

if(BUILD_TESTING)
    add_executable(FengYinSf2VoiceTests tests/test_sf2_voice.cpp)
    target_include_directories(FengYinSf2VoiceTests PRIVATE native/Sf2)
    target_link_libraries(FengYinSf2VoiceTests PRIVATE libfluidsynth)
    add_executable(FengYinSf2PublicTests tests/test_sf2_public.cpp)
    target_include_directories(FengYinSf2PublicTests PRIVATE native/Sf2)
    target_link_libraries(FengYinSf2PublicTests PRIVATE libfluidsynth)
    if(WIN32)
        foreach(test FengYinSf2VoiceTests FengYinSf2PublicTests)
            add_custom_command(TARGET ${test} POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
                $<TARGET_FILE:libfluidsynth> $<TARGET_FILE_DIR:${test}>)
        endforeach()
    endif()
endif()

# The private VST3 travels with the application, never in a system-wide plug-in directory.
add_dependencies(FengYin FengYinSF2_VST3)
add_custom_command(TARGET FengYin POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:FengYin>/components/SF2"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "$<GENEX_EVAL:$<TARGET_PROPERTY:FengYinSF2_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>"
        "$<TARGET_FILE_DIR:FengYin>/components/SF2/FengYin SF2.vst3")
if(WIN32)
    # JUCE's VST3 LoadLibrary resolves imports from the application directory,
    # not from the loaded plug-in directory. Also retain the Probe's local copy.
    add_custom_command(TARGET FengYin POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:libfluidsynth> $<TARGET_FILE_DIR:FengYin>)
endif()

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/sf2-fixture.sf2"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/create_sf2_fixture.py" "${CMAKE_BINARY_DIR}/sf2-fixture.sf2"
        DEPENDS tests/create_sf2_fixture.py)
    add_custom_target(FengYinSf2Fixture DEPENDS "${CMAKE_BINARY_DIR}/sf2-fixture.sf2")
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/sf2-fast-attack.sf2"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/create_sf2_fixture.py" "${CMAKE_BINARY_DIR}/sf2-fast-attack.sf2" --fast-attack
        DEPENDS tests/create_sf2_fixture.py)
    add_custom_target(FengYinSf2FastAttackFixture DEPENDS "${CMAKE_BINARY_DIR}/sf2-fast-attack.sf2")
    add_dependencies(FengYinSf2VoiceTests FengYinSf2Fixture FengYinSf2FastAttackFixture)
    target_sources(FengYinDjHostTests PRIVATE tests/test_sf2_host.cpp)
    add_dependencies(FengYinDjHostTests FengYinSF2_VST3 FengYinSf2Fixture)
    add_test(NAME sf2_voice_pcm COMMAND FengYinSf2VoiceTests "${CMAKE_BINARY_DIR}/sf2-fixture.sf2" "${CMAKE_BINARY_DIR}/sf2-fast-attack.sf2")
    add_test(NAME sf2_production_host COMMAND FengYinDjHostTests --sf2
        "$<GENEX_EVAL:$<TARGET_PROPERTY:FengYinSF2_VST3,JUCE_PLUGIN_ARTEFACT_FILE>>" "${CMAKE_BINARY_DIR}/sf2-fixture.sf2")
    set_tests_properties(sf2_production_host PROPERTIES TIMEOUT 120)
    add_executable(FengYinBlePairingPolicyTests tests/test_ble_pairing_policy.cpp)
    target_include_directories(FengYinBlePairingPolicyTests PRIVATE native/Source)
    target_link_libraries(FengYinBlePairingPolicyTests PRIVATE juce::juce_core)
    add_test(NAME ble_pairing_model_isolation COMMAND FengYinBlePairingPolicyTests)
    add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/sf2-multiple.sf2"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tests/create_sf2_fixture.py" "${CMAKE_BINARY_DIR}/sf2-multiple.sf2" --multiple
        DEPENDS tests/create_sf2_fixture.py)
    add_custom_target(FengYinSf2MultipleFixture DEPENDS "${CMAKE_BINARY_DIR}/sf2-multiple.sf2")
    juce_add_console_app(FengYinSf2LifecycleTests PRODUCT_NAME FengYinSf2LifecycleTests)
    target_sources(FengYinSf2LifecycleTests PRIVATE tests/test_sf2_lifecycle.cpp)
    target_include_directories(FengYinSf2LifecycleTests PRIVATE native/Sf2 native/Source)
    target_compile_definitions(FengYinSf2LifecycleTests PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(FengYinSf2LifecycleTests PRIVATE FengYinSF2 libfluidsynth juce::juce_audio_utils juce::juce_cryptography)
    add_dependencies(FengYinSf2LifecycleTests FengYinSf2MultipleFixture FengYinSf2Fixture FengYinSF2Probe)
    add_custom_command(TARGET FengYinSf2LifecycleTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:FengYinSF2Probe> $<TARGET_FILE_DIR:FengYinSf2LifecycleTests>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:libfluidsynth> $<TARGET_FILE_DIR:FengYinSf2LifecycleTests>)
    add_test(NAME sf2_lifecycle COMMAND FengYinSf2LifecycleTests "${CMAKE_BINARY_DIR}/sf2-fixture.sf2" "${CMAKE_BINARY_DIR}/sf2-multiple.sf2")
    set_tests_properties(sf2_lifecycle PROPERTIES TIMEOUT 120)
endif()

add_custom_command(TARGET FengYin POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_SOURCE_DIR}/third_party/sf2" "$<TARGET_FILE_DIR:FengYin>/components/SF2/licenses")
