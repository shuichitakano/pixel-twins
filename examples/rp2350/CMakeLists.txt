cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED ENV{PICO_SDK_PATH})
    message(FATAL_ERROR "PICO_SDK_PATHにPico SDKのパスを設定してください")
endif()
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(pixel_twins_rp2350_smoke LANGUAGES C CXX ASM)
set(PICO_BOARD pico2 CACHE STRING "RP2350搭載ボード")
pico_sdk_init()

set(PIXEL_TWINS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PIXEL_TWINS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(../.. pixel-twins)

add_executable(pixel_twins_rp2350_smoke main.cpp)
target_link_libraries(pixel_twins_rp2350_smoke PRIVATE pixel_twins::rp2350)
target_compile_features(pixel_twins_rp2350_smoke PRIVATE cxx_std_17)
pico_add_extra_outputs(pixel_twins_rp2350_smoke)
