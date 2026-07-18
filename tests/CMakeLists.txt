add_executable(pixel_twins_tests
    framebuffer_test.cpp
)
target_link_libraries(pixel_twins_tests PRIVATE pixel_twins::pixel_twins)

add_test(NAME pixel_twins_tests COMMAND pixel_twins_tests)
