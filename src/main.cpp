#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/cyw43_arch.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"
#include "hardware/pll.h"

#include "bt.h"
#include "usb.h"
#include "audio.h"
#include "config.h"
#include "cmd.h"
#if ENABLE_BATT_LED
#include "battery_led.h"
#endif

static void init_overclock(void) {
    vreg_set_voltage(VREG_VOLTAGE_1_30);
    sleep_ms(2);
    set_sys_clock_pll(1440000000, 4, 1);
    setup_default_uart();
}

int main(void) {
    init_overclock();
    sleep_ms(10);
    stdio_init_all();

#if ENABLE_SERIAL
    sleep_ms(500);
    printf("[DS5 Bridge] Booting at %lu MHz\n", clock_get_hz(clk_sys) / 1000000);
#endif

    config_init();

    // Initialize USB (calls tusb_init internally)
    usb_init();

    if (cyw43_arch_init()) {
#if ENABLE_SERIAL
        printf("[ERROR] CYW43 init failed\n");
#endif
        while (1) {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
            sleep_ms(100);
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            sleep_ms(100);
        }
    }

    bt_init();
    audio_init();

#if ENABLE_SERIAL
    printf("[DS5 Bridge] Init complete. Scanning for DualSense...\n");
#endif

    while (1) {
        usb_task();             // tud_task() + HID report forwarding
        cyw43_arch_poll();
        bt_poll();
        audio_loop();
#if ENABLE_BATT_LED
        battery_led_tick();
#endif
    }

    return 0;
}
