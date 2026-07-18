cmake_minimum_required(VERSION 3.16)

project(pixel_twins VERSION 0.1.0 LANGUAGES CXX)

option(PIXEL_TWINS_BUILD_EXAMPLES "Pixel Twinsのサンプルをビルドする" ON)
option(PIXEL_TWINS_BUILD_TESTS "Pixel Twinsのテストをビルドする" ON)

add_library(pixel_twins
    src/background.cpp
    src/font.cpp
    src/framebuffer.cpp
    src/primitives.cpp
    src/render_target.cpp
    src/sprite.cpp
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

if(PIXEL_TWINS_BUILD_EXAMPLES)
    add_subdirectory(platform/sdl)
    add_subdirectory(examples/framebuffer)
endif()

if(PIXEL_TWINS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
