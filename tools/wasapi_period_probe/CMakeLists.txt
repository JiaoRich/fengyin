cmake_minimum_required(VERSION 3.22)

project(FengYinWasapiPeriodProbe VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    add_compile_options(/utf-8 /W4 /permissive-)
endif()

add_executable(FengYinWasapiPeriodProbe WIN32 main.cpp)
target_compile_definitions(FengYinWasapiPeriodProbe PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)
target_link_libraries(FengYinWasapiPeriodProbe PRIVATE ole32 uuid)

