add_executable(pixel_twins_framebuffer_example
    main.cpp
)
target_link_libraries(pixel_twins_framebuffer_example
    PRIVATE pixel_twins::sdl
)
