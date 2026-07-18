cmake_minimum_required(VERSION 3.16)

project(pixel_twins VERSION 0.1.0 LANGUAGES CXX)

if(PICO_SDK_VERSION_STRING)
    set(_pixel_twins_host_default OFF)
else()
    set(_pixel_twins_host_default ON)
endif()
option(PIXEL_TWINS_BUILD_EXAMPLES "Pixel TwinsのSDLサンプルをビルドする" ${_pixel_twins_host_default})
option(PIXEL_TWINS_BUILD_TESTS "Pixel Twinsのホストテストをビルドする" ${_pixel_twins_host_default})

add_library(pixel_twins
    src/audio_system.cpp
    src/background.cpp
    src/background_asset.cpp
    src/controller.cpp
    src/font.cpp
    src/framebuffer.cpp
    src/primitives.cpp
    src/render_target.cpp
    src/sequencer.cpp
    src/sound.cpp
    src/sound_waves.cpp
    src/sprite.cpp
    src/sprite_asset.cpp
)
add_library(pixel_twins::pixel_twins ALIAS pixel_twins)

target_include_directories(pixel_twins
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)
target_compile_features(pixel_twins PUBLIC cxx_std_17)
set_target_properties(pixel_twins PROPERTIES
    CXX_EXTENSIONS OFF
)

if(PICO_SDK_VERSION_STRING)
    # 共通ソース自体をRP2350用定義でコンパイルし、SRAM配置属性を有効にする。
    target_link_libraries(pixel_twins PUBLIC pico_platform)
    add_subdirectory(platform/rp2350)
endif()

if(PIXEL_TWINS_BUILD_EXAMPLES)
    add_subdirectory(platform/sdl)
    add_subdirectory(examples/framebuffer)
endif()

if(PIXEL_TWINS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
