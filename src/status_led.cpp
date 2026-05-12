//
// GPIO23 status LED driver — Waveshare RP2350B Plus W
// See status_led.h for full description.
//

#include "status_led.h"
#include "bt.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include <cstdio>

// GPIO23 = LED2 on Waveshare RP2350B Plus W (direct RP2350B GPIO).
// Defined in boards/waveshare_rp2350b_plus_w.h as PICO_DEFAULT_LED_PIN.
// We use the literal here to be explicit — do not substitute CYW43_WL_GPIO_LED_PIN.
static constexpr uint GPIO23_LED = 23u;

// Pre-disconnect blink parameters.
// 4 Hz = 250 ms period = 125 ms per half-cycle.
// 3 full on/off cycles = 6 half-cycles before disconnect fires.
static constexpr uint64_t WARN_HALF_PERIOD_US = 125'000;  // 125 ms
static constexpr uint32_t WARN_HALF_CYCLES    = 6;        // 3 full blinks

namespace {

enum class LedState {
    BOOT_ON,        // solid ON — waiting for CYW43 init
    IDLE,           // OFF — normal operation
    PRE_DISCONNECT  // blinking — warning user of imminent disconnect
};

LedState state          = LedState::BOOT_ON;
uint64_t last_toggle_us = 0;
uint32_t half_cycles    = 0;
bool     led_on         = false;

void set_gpio23(bool on) {
    gpio_put(GPIO23_LED, on);
    led_on = on;
}

} // namespace

void status_led_init(void) {
    gpio_init(GPIO23_LED);
    gpio_set_dir(GPIO23_LED, GPIO_OUT);
    set_gpio23(true);   // ON immediately — board is alive
    state       = LedState::BOOT_ON;
    half_cycles = 0;
    printf("[StatusLED] Boot ON\n");
}

void status_led_cyw43_ready(void) {
    // Only transition from BOOT_ON — do not interrupt a warning in progress.
    if (state == LedState::BOOT_ON) {
        set_gpio23(false);
        state = LedState::IDLE;
        printf("[StatusLED] CYW43 ready — boot LED off\n");
    }
}

void status_led_tick(void) {
    if (state != LedState::PRE_DISCONNECT) return;

    const uint64_t now = time_us_64();
    if ((now - last_toggle_us) < WARN_HALF_PERIOD_US) return;

    last_toggle_us = now;
    half_cycles++;

    // Toggle GPIO23 for the blink
    set_gpio23(!led_on);

    if (half_cycles >= WARN_HALF_CYCLES) {
        // Warning complete — ensure LED is OFF then disconnect
        set_gpio23(false);
        state = LedState::IDLE;
        printf("[StatusLED] Disconnect warning complete — disconnecting\n");
        bt_disconnect();
    }
}

void status_led_warn_disconnect(void) {
    // Re-arm only if idle — do not restart a blink already in progress
    if (state != LedState::IDLE) return;

    state          = LedState::PRE_DISCONNECT;
    half_cycles    = 0;
    last_toggle_us = time_us_64();
    set_gpio23(true);   // start with LED ON (first half-cycle)
    printf("[StatusLED] Disconnect warning armed (1.5 s)\n");
}

bool status_led_disconnect_pending(void) {
    return state == LedState::PRE_DISCONNECT;
}
