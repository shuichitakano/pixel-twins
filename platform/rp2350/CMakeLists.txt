if(NOT PICO_PLATFORM MATCHES "^rp2350")
    message(FATAL_ERROR "Pixel Twinsの実機版はRP2350のみを対象とします")
endif()

# ボード固有ドライバが共通コードとPico SDKに依存するための境界。
add_library(pixel_twins_rp2350
    led_panel.cpp
)
add_library(pixel_twins::rp2350 ALIAS pixel_twins_rp2350)
target_link_libraries(pixel_twins_rp2350
    PUBLIC pixel_twins::pixel_twins pico_stdlib
    PRIVATE hardware_clocks hardware_dma hardware_gpio hardware_pio
)
target_include_directories(pixel_twins_rp2350 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_compile_definitions(pixel_twins_rp2350 PUBLIC PIXEL_TWINS_PLATFORM_RP2350=1)
target_compile_features(pixel_twins_rp2350 PUBLIC cxx_std_17)

pico_generate_pio_header(pixel_twins_rp2350
    ${CMAKE_CURRENT_SOURCE_DIR}/led_panel.pio
)
