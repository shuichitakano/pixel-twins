if(NOT PICO_PLATFORM MATCHES "^rp2350")
    message(FATAL_ERROR "Pixel Twinsの実機版はRP2350のみを対象とします")
endif()

# ボード固有ドライバが共通コードとPico SDKに依存するための境界。
# ピン配置とPIO/DMA方式の確定後、実装ソースをこのターゲットに追加する。
add_library(pixel_twins_rp2350 INTERFACE)
add_library(pixel_twins::rp2350 ALIAS pixel_twins_rp2350)
target_link_libraries(pixel_twins_rp2350 INTERFACE pixel_twins::pixel_twins pico_stdlib)
target_compile_definitions(pixel_twins_rp2350 INTERFACE PIXEL_TWINS_PLATFORM_RP2350=1)
