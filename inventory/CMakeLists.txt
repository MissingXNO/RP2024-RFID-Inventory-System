cmake_minimum_required(VERSION 3.13)

set(PICO_BOARD "pico_w")

# initialize the SDK based on PICO_SDK_PATH
# note: this must happen before project()
include(pico_sdk_import.cmake)

project(inventory)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(inventory inventory.c mfrc522.c pico_keypad4x4.c)

# Add pico_stdlib library which aggregates commonly used features
target_link_libraries(inventory PRIVATE pico_stdlib pico_flash pico_binary_info pico_cyw43_arch_none pico_stdio_usb 
hardware_gpio hardware_flash hardware_i2c hardware_spi hardware_timer hardware_pwm hardware_irq hardware_sync
)

# Enable USB serial
pico_enable_stdio_usb(inventory 1)
pico_enable_stdio_uart(inventory 0)

# create map/bin/hex/uf2 file in addition to ELF.
pico_add_extra_outputs(inventory)