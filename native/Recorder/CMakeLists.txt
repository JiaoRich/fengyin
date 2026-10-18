set(FENGYIN_RECORDING_ROOT "${CMAKE_SOURCE_DIR}/third_party/recording" CACHE PATH "Verified recording dependencies")
set(SRL_ROOT "${FENGYIN_RECORDING_ROOT}/srl/ScreenRecorderLibNative")
if(NOT EXISTS "${SRL_ROOT}/RecordingManager.cpp" OR NOT EXISTS "${FENGYIN_RECORDING_ROOT}/miniaudio/miniaudio.h")
    message(FATAL_ERROR "Run scripts/prepare_recording.py before configuring the Windows recorder")
endif()
# The upstream directory also contains retired source (e.g. Resizer.cpp) that
# is not part of its release project. Use its verified compile list verbatim.
include("${SRL_ROOT}/upstream-sources.cmake")
find_program(FENGYIN_FXC fxc HINTS
    "$ENV{WindowsSdkDir}/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
    "C:/Program Files (x86)/Windows Kits/10/bin/${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}/x64"
    REQUIRED)
set(SRL_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/shaders")
file(MAKE_DIRECTORY "${SRL_GENERATED}")
foreach(shader Pixel Vertex)
    if(shader STREQUAL "Pixel")
        set(entry PS)
        set(profile ps_4_0_level_9_1)
    else()
        set(entry VS)
        set(profile vs_4_0_level_9_1)
    endif()
    add_custom_command(OUTPUT "${SRL_GENERATED}/${shader}Shader.h"
        COMMAND "${FENGYIN_FXC}" /nologo /T ${profile} /E ${entry} /Vn g_${entry}
            /Fh "${SRL_GENERATED}/${shader}Shader.h" "${SRL_ROOT}/${shader}Shader.hlsl"
        DEPENDS "${SRL_ROOT}/${shader}Shader.hlsl" VERBATIM)
endforeach()
add_library(FengYinScreenRecorderLib STATIC ${SRL_SOURCES}
    "${SRL_GENERATED}/PixelShader.h" "${SRL_GENERATED}/VertexShader.h")
set_target_properties(FengYinScreenRecorderLib PROPERTIES CXX_STANDARD 17)
target_include_directories(FengYinScreenRecorderLib PUBLIC "${SRL_ROOT}" PRIVATE "${SRL_GENERATED}")
target_compile_definitions(FengYinScreenRecorderLib PUBLIC NOMINMAX UNICODE _UNICODE _HAS_STD_BYTE=0
    _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR WINVER=0x0A00 _WIN32_WINNT=0x0A00)
target_compile_definitions(FengYinScreenRecorderLib PRIVATE SCREENRECORDERLIBNATIVE_EXPORTS)
target_compile_options(FengYinScreenRecorderLib PRIVATE /EHa /utf-8)
target_link_libraries(FengYinScreenRecorderLib PUBLIC windowsapp d3d11 dxgi dxguid dwmapi
    mf mfplat mfreadwrite mfuuid evr wmcodecdspuuid ole32 oleaut32 uuid shlwapi shell32 winmm windowscodecs avrt)
add_executable(FengYinRecorder WIN32 main.cpp MiniAudioRecorder.cpp ../Source/WindowsCompatibility.manifest)
set_target_properties(FengYinRecorder PROPERTIES CXX_STANDARD 17
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
target_include_directories(FengYinRecorder PRIVATE "${FENGYIN_RECORDING_ROOT}/miniaudio")
target_compile_options(FengYinRecorder PRIVATE /utf-8 /EHa)
target_link_libraries(FengYinRecorder PRIVATE FengYinScreenRecorderLib)
install(TARGETS FengYinRecorder RUNTIME DESTINATION .)
