cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if (WIN32)
    set(USERHOME $ENV{USERPROFILE})
else ()
    set(USERHOME $ENV{HOME})
endif ()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif ()

set(PICO_BOARD waveshare_rp2350b_plus_w CACHE STRING "Board type")
set(PICO_BOARD_HEADER_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/boards)

option(ENABLE_SERIAL "Enable USB CDC serial printf output" OFF)
if (ENABLE_SERIAL)
    set(ENABLE_SERIAL_VALUE 1)
else ()
    set(ENABLE_SERIAL_VALUE 0)
endif ()

option(ENABLE_VERBOSE "Enable Log Verbose" OFF)
if (ENABLE_VERBOSE)
    set(ENABLE_VERBOSE_VALUE 1)
else ()
    set(ENABLE_VERBOSE_VALUE 0)
endif ()

option(ENABLE_BATT_LED "Blink Pico LED when DualSense battery is low (<=10%)" ON)
if (ENABLE_BATT_LED)
    set(ENABLE_BATT_LED_VALUE 1)
else ()
    set(ENABLE_BATT_LED_VALUE 0)
endif ()

# Opus encoder complexity (0=lowest CPU, 10=highest quality).
# At 360 MHz Core 1 handles complexity 4 comfortably.
# Reduce to 2 if you observe audio dropouts on your specific unit.
set(OPUS_COMPLEXITY 4 CACHE STRING "Opus encoder complexity (0-10)")

include(pico_sdk_import.cmake)

project(ds5-bridge C CXX ASM)

pico_sdk_init()

# ── Flash size guard ──────────────────────────────────────────────────────────
# The Waveshare RP2350B Plus W has a W25Q128JVSIQ = 16 MB flash.
# boards/waveshare_rp2350b_plus_w.h defines PICO_FLASH_SIZE_BYTES = 16777216.
# config.cpp anchors ALL flash sector offsets to PICO_FLASH_SIZE_BYTES.
# If this value is wrong, config/profile writes will corrupt the firmware.
# This guard fires at CMake configure time if the value is incorrect.
if (DEFINED PICO_FLASH_SIZE_BYTES)
    if (NOT PICO_FLASH_SIZE_BYTES EQUAL 16777216)
        message(FATAL_ERROR
            "PICO_FLASH_SIZE_BYTES=${PICO_FLASH_SIZE_BYTES} but the Waveshare "
            "RP2350B Plus W has 16 MB flash (16777216 bytes). "
            "Check boards/waveshare_rp2350b_plus_w.h.")
    endif ()
else ()
    message(WARNING
        "PICO_FLASH_SIZE_BYTES is not visible at CMake configure time. "
        "Ensure boards/waveshare_rp2350b_plus_w.h defines it as 16777216.")
endif ()

add_executable(ds5-bridge
        src/usb_descriptors.cpp
        src/main.cpp
        src/usb.cpp
        src/audio.cpp
        src/bt.cpp
        src/config.cpp
        src/cmd.cpp
)

if (ENABLE_BATT_LED)
    target_sources(ds5-bridge PRIVATE src/battery_led.cpp)
endif ()

add_library(wdl_resampler STATIC
        lib/WDL/WDL/resample.cpp
)
target_include_directories(wdl_resampler PUBLIC
        lib/WDL/WDL
)

# Opus build options — fixed-point codec with float API for opus_encode_float()
set(OPUS_DISABLE_DEEP_PLC ON CACHE BOOL "" FORCE)
set(OPUS_DISABLE_DRED ON CACHE BOOL "" FORCE)
set(OPUS_DISABLE_OSCE ON CACHE BOOL "" FORCE)
set(OPUS_FIXED_POINT ON CACHE BOOL "" FORCE)
set(OPUS_ENABLE_FLOAT_API ON CACHE BOOL "" FORCE)
set(OPUS_HARDENING OFF CACHE BOOL "" FORCE)
set(OPUS_STACK_PROTECTOR OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_PKG_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_INSTALL_CMAKE_CONFIG_MODULE OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(OPUS_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(lib/opus)

target_compile_options(wdl_resampler PRIVATE -fsigned-char)
target_compile_options(ds5-bridge PRIVATE -fsigned-char)

target_compile_definitions(wdl_resampler PRIVATE
        WDL_RESAMPLE_TYPE=float
)
target_compile_definitions(ds5-bridge PRIVATE
        CYW43_LWIP=0
        ENABLE_SERIAL=${ENABLE_SERIAL_VALUE}
        # Core 1 runs entirely from SRAM (Opus encode loop + blocking queue wait).
        # It never reads or writes flash, so Core 0 flash operations are safe
        # without stopping Core 1. Do NOT add any flash access to core1_entry().
        PICO_FLASH_ASSUME_CORE1_SAFE=1
        WDL_RESAMPLE_TYPE=float
        # CYW43439 SPI interface maximum clock is ~50 MHz.
        # CYW43 SPI clock = sys_clock / CYW43_PIO_CLOCK_DIV_INT
        # At 360 MHz sys_clock:
        #   divider 5 → 72.0 MHz  OVER SPEC — causes BT corruption/disconnects
        #   divider 6 → 60.0 MHz  marginal
        #   divider 7 → 51.4 MHz  marginal
        #   divider 8 → 45.0 MHz  within spec — used here
        CYW43_PIO_CLOCK_DIV_INT=8
        PICO_STDIO_USB_USE_DEFAULT_DESCRIPTORS=0
        PICO_STDIO_USB_ENABLE_RESET_VIA_BAUD_RATE=0
        PICO_STDIO_USB_ENABLE_RESET_VIA_VENDOR_INTERFACE=0
        ENABLE_VERBOSE=${ENABLE_VERBOSE_VALUE}
        ENABLE_BATT_LED=${ENABLE_BATT_LED_VALUE}
        OPUS_COMPLEXITY=${OPUS_COMPLEXITY}
)

pico_set_program_name(ds5-bridge "ds5-bridge")
pico_set_program_version(ds5-bridge "0.5.3")

pico_enable_stdio_uart(ds5-bridge 1)
pico_enable_stdio_usb(ds5-bridge ${ENABLE_SERIAL_VALUE})

target_include_directories(${PROJECT_NAME} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ds5-bridge
        pico_stdlib
        pico_multicore
        hardware_watchdog
        hardware_timer
        hardware_flash
        hardware_vreg
        hardware_clocks
        hardware_pll
        hardware_sync
        pico_btstack_classic
        pico_cyw43_arch_poll
        pico_btstack_cyw43
        tinyusb_device
        tinyusb_board
        wdl_resampler
        opus
)

pico_add_extra_outputs(ds5-bridge)
