cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(PICO_BOARD pico_w CACHE STRING "Board type")

include(pico_sdk_import.cmake) # Set with the path of the file in your pico-sdk

project(blink_ble C CXX ASM)

pico_sdk_init()

add_executable(blink_ble blink_ble.c )

pico_set_program_name(blink_ble "blink_ble")
pico_set_program_version(blink_ble "0.1")

pico_enable_stdio_uart(blink_ble 0)
pico_enable_stdio_usb(blink_ble 1)

target_link_libraries(blink_ble
        pico_stdlib
        pico_btstack_ble
        pico_btstack_cyw43
        pico_cyw43_arch_none
        )

target_include_directories(blink_ble PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/../config
)

pico_btstack_make_gatt_header(blink_ble PRIVATE "${CMAKE_CURRENT_LIST_DIR}/blink_ble.gatt")

pico_add_extra_outputs(blink_ble)
