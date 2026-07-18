find_package(SDL3 CONFIG REQUIRED)

add_library(pixel_twins_sdl
    sdl_presenter.cpp
)
add_library(pixel_twins::sdl ALIAS pixel_twins_sdl)

target_include_directories(pixel_twins_sdl
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(pixel_twins_sdl PUBLIC cxx_std_17)
target_link_libraries(pixel_twins_sdl
    PUBLIC pixel_twins::pixel_twins
    PRIVATE SDL3::SDL3
)
set_target_properties(pixel_twins_sdl PROPERTIES
    CXX_EXTENSIONS OFF
)
