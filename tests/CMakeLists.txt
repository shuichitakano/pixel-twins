add_executable(pixel_twins_tests
    framebuffer_test.cpp
)
target_link_libraries(pixel_twins_tests PRIVATE pixel_twins::pixel_twins)

add_test(NAME pixel_twins_tests COMMAND pixel_twins_tests)

add_executable(pixel_twins_sprite_tests
    sprite_test.cpp
)
target_link_libraries(pixel_twins_sprite_tests PRIVATE pixel_twins::pixel_twins)

add_test(NAME pixel_twins_sprite_tests COMMAND pixel_twins_sprite_tests)

add_executable(pixel_twins_background_tests
    background_test.cpp
)
target_link_libraries(pixel_twins_background_tests PRIVATE pixel_twins::pixel_twins)

add_test(NAME pixel_twins_background_tests COMMAND pixel_twins_background_tests)

add_executable(pixel_twins_primitives_tests
    primitives_test.cpp
)
target_link_libraries(pixel_twins_primitives_tests PRIVATE pixel_twins::pixel_twins)

add_test(NAME pixel_twins_primitives_tests COMMAND pixel_twins_primitives_tests)

add_executable(pixel_twins_font_tests
    font_test.cpp
)
target_link_libraries(pixel_twins_font_tests PRIVATE pixel_twins::pixel_twins)

add_test(NAME pixel_twins_font_tests COMMAND pixel_twins_font_tests)
add_executable(controller_test controller_test.cpp)
target_link_libraries(controller_test PRIVATE pixel_twins::pixel_twins)
add_test(NAME controller_test COMMAND controller_test)
