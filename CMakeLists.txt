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

set(PICO_BOARD pico2_w CACHE STRING "Board type")

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

# Blink onboard LED at 1Hz when DualSense battery <= 10% and discharging.
# ON by default. Pass -DENABLE_BATT_LED=OFF to disable.
option(ENABLE_BATT_LED "Blink Pico LED when DualSense battery is low (<=10%)" ON)
if (ENABLE_BATT_LED)
    set(ENABLE_BATT_LED_VALUE 1)
else ()
    set(ENABLE_BATT_LED_VALUE 0)
endif ()

# Opus encoder complexity: 0=lowest CPU, 10=highest quality
set(OPUS_COMPLEXITY 0 CACHE STRING "Opus encoder complexity (0-10)")

include(pico_sdk_import.cmake)

project(ds5-bridge C CXX ASM)

pico_sdk_init()

add_executable(ds5-bridge
        src/usb_descriptors.cpp
        src/main.cpp
        src/usb.cpp
        src/audio.cpp
        src/bt.cpp
        src/config.cpp
        src/cmd.cpp
)

# battery_led.cpp is only compiled when ENABLE_BATT_LED is ON
if (ENABLE_BATT_LED)
    target_sources(ds5-bridge PRIVATE src/battery_led.cpp)
endif ()

add_library(wdl_resampler STATIC
        lib/WDL/WDL/resample.cpp
)
target_include_directories(wdl_resampler PUBLIC
        lib/WDL/WDL
)

add_subdirectory(lib/opus)

target_compile_options(wdl_resampler PRIVATE -fsigned-char)
target_compile_options(ds5-bridge PRIVATE -fsigned-char)

target_compile_definitions(wdl_resampler PRIVATE
        WDL_RESAMPLE_TYPE=float
)
target_compile_definitions(ds5-bridge PRIVATE
        CYW43_LWIP=0
        ENABLE_SERIAL=${ENABLE_SERIAL_VALUE}
        PICO_FLASH_ASSUME_CORE1_SAFE=1
        WDL_RESAMPLE_TYPE=float
        CYW43_PIO_CLOCK_DIV_INT=5
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
        pico_btstack_classic
        pico_cyw43_arch_poll
        pico_btstack_cyw43
        tinyusb_device
        tinyusb_board
        wdl_resampler
        opus
)

pico_add_extra_outputs(ds5-bridge)
