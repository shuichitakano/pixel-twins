if(NOT PICO_PLATFORM MATCHES "^rp2350")
    message(FATAL_ERROR "Pixel Twinsの実機版はRP2350のみを対象とします")
endif()

set(PIXEL_TWINS_PICO_PIO_USB_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../external/Pico-PIO-USB"
    CACHE PATH "Pico-PIO-USBのソースディレクトリ")
if(NOT EXISTS "${PIXEL_TWINS_PICO_PIO_USB_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "Pico-PIO-USBがありません。git submodule update --init --recursiveを実行してください")
endif()
if(NOT TARGET pico_pio_usb)
    add_subdirectory(
        "${PIXEL_TWINS_PICO_PIO_USB_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}/Pico-PIO-USB")
endif()

set(_pixel_twins_dual_usbh
    "${CMAKE_CURRENT_BINARY_DIR}/generated/usbh_dual.c")
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        "-DINPUT=${PICO_TINYUSB_PATH}/src/host/usbh.c"
        "-DOUTPUT=${_pixel_twins_dual_usbh}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/generate_dual_usbh.cmake"
    RESULT_VARIABLE _pixel_twins_dual_usbh_result
)
if(NOT _pixel_twins_dual_usbh_result EQUAL 0)
    message(FATAL_ERROR "TinyUSBのdual host対応ソースを生成できませんでした")
endif()

# ボード固有ドライバが共通コードとPico SDKに依存するための境界。
add_library(pixel_twins_rp2350
    led_panel.cpp
    sony_hid.cpp
    usb_controller.cpp
    dual_hcd.c
    native_hcd_wrapper.c
    pio_hcd_wrapper.c
    ${_pixel_twins_dual_usbh}
    ${PICO_TINYUSB_PATH}/src/portable/raspberrypi/rp2040/rp2040_usb.c
    ${PICO_TINYUSB_PATH}/src/class/hid/hid_host.c
)
add_library(pixel_twins::rp2350 ALIAS pixel_twins_rp2350)
target_link_libraries(pixel_twins_rp2350
    PUBLIC pixel_twins::pixel_twins pico_stdlib
    PRIVATE tinyusb_common
    PRIVATE pico_pio_usb
    PRIVATE hardware_clocks hardware_dma hardware_gpio hardware_pio
)
target_include_directories(pixel_twins_rp2350 PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)
target_include_directories(pixel_twins_rp2350 PRIVATE
    ${PICO_TINYUSB_PATH}/src/host
)
target_compile_definitions(pixel_twins_rp2350
    PUBLIC
        PIXEL_TWINS_PLATFORM_RP2350=1
        PIO_USB_USE_TINYUSB
    PRIVATE
        RP2040_USB_HOST_MODE=1
)
target_compile_features(pixel_twins_rp2350 PUBLIC cxx_std_17)

pico_generate_pio_header(pixel_twins_rp2350
    ${CMAKE_CURRENT_SOURCE_DIR}/led_panel.pio
)
