#ifndef _BOARDS_WAVESHARE_RP2350B_PLUS_W_H
#define _BOARDS_WAVESHARE_RP2350B_PLUS_W_H

// --- Flash ---
#define PICO_FLASH_SIZE_BYTES (16 * 1024 * 1024)
// Faster flash clock: 150MHz sys_clk / 4 = 37.5MHz SPI (within spec for W25Q128)
#define PICO_FLASH_SPI_CLKDIV 4

// --- Crystal ---
#define PICO_XOSC_STARTUP_DELAY_MULTIPLIER 64

// --- CYW43 (Bluetooth/WiFi combo on SPI) ---
#define CYW43_WL_GPIO_COUNT 3
#define CYW43_WL_GPIO_LED_PIN 0

#define PICO_CYW43_WL_GPIO_ON_PIN 23
#define PICO_CYW43_WL_GPIO_DATA_PIN 24
#define PICO_CYW43_WL_GPIO_CLK_PIN 29
#define PICO_CYW43_WL_GPIO_CS_PIN 25

// --- PSRAM (optional, active-low CS on GPIO47) ---
#define PICO_PSRAM_CS_PIN 47

// --- Default UART (disabled in this project but defined for completeness) ---
#define PICO_DEFAULT_UART 0
#define PICO_DEFAULT_UART_TX_PIN 0
#define PICO_DEFAULT_UART_RX_PIN 1

// --- Default LED is on CYW43 ---
#define PICO_DEFAULT_LED_PIN_INVERTED 0
#define CYW43_DEFAULT_LED_GPIO 0

// --- Board identification ---
#define PICO_RP2350_B2_SUPPORTED 1

#endif // _BOARDS_WAVESHARE_RP2350B_PLUS_W_H
