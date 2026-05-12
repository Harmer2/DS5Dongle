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
// Do not substitute CYW43_WL_GPIO_LED_PIN here — that is a different LED
// routed through the CYW43439 driver and requires cyw43_arch_init() first.
static constexpr uint GPIO23_LED = 23u;

// Blink timing: 4 Hz = 250 ms period = 125 ms per half-cycle.
static constexpr uint64_t BLINK_HALF_PERIOD_US = 125'000;

// Pre-disconnect: 3 full on/off cycles = 6 half-cycles.
static constexpr uint32_t WARN_HALF_CYCLES = 6u;

// Post-disconnect blink counts (full on/off cycles → half-cycles).
static constexpr uint32_t POST_MANUAL_HALF_CYCLES    = 4u;  // 2 blinks × 2
static constexpr uint32_t POST_ERROR_HALF_CYCLES     = 8u;  // 4 blinks × 2

namespace {

enum class LedState {
    BOOT_ON,
    IDLE,
    PRE_DISCONNECT,   // inactivity warning — calls bt_disconnect() when done
    POST_DISCONNECT   // reason-coded blink after HCI disconnect — no bt_disconnect()
};

LedState state          = LedState::BOOT_ON;
uint64_t last_toggle_us = 0;
uint32_t half_cycles    = 0;
uint32_t target_half_cycles = 0;  // set when entering POST_DISCONNECT
bool     led_on         = false;

void set_gpio23(bool on) {
    gpio_put(GPIO23_LED, on);
    led_on = on;
}

} // namespace

void status_led_init(void) {
    gpio_init(GPIO23_LED);
    gpio_set_dir(GPIO23_LED, GPIO_OUT);
    set_gpio23(true);
    state               = LedState::BOOT_ON;
    half_cycles         = 0;
    target_half_cycles  = 0;
    printf("[StatusLED] Boot ON\n");
}

void status_led_cyw43_ready(void) {
    if (state == LedState::BOOT_ON) {
        set_gpio23(false);
        state = LedState::IDLE;
        printf("[StatusLED] CYW43 ready — boot LED off\n");
    }
}

void status_led_tick(void) {
    if (state != LedState::PRE_DISCONNECT && state != LedState::POST_DISCONNECT) return;

    const uint64_t now = time_us_64();
    if ((now - last_toggle_us) < BLINK_HALF_PERIOD_US) return;

    last_toggle_us = now;
    half_cycles++;

    set_gpio23(!led_on);

    if (state == LedState::PRE_DISCONNECT) {
        if (half_cycles >= WARN_HALF_CYCLES) {
            set_gpio23(false);
            state = LedState::IDLE;
            printf("[StatusLED] Disconnect warning complete — disconnecting\n");
            bt_disconnect();
        }
    } else {
        // POST_DISCONNECT
        if (half_cycles >= target_half_cycles) {
            set_gpio23(false);
            state = LedState::IDLE;
            printf("[StatusLED] Post-disconnect blink complete\n");
        }
    }
}

void status_led_warn_disconnect(void) {
    if (state != LedState::IDLE) return;

    state          = LedState::PRE_DISCONNECT;
    half_cycles    = 0;
    last_toggle_us = time_us_64();
    set_gpio23(true);
    printf("[StatusLED] Disconnect warning armed (3× blink)\n");
}

bool status_led_disconnect_pending(void) {
    return state == LedState::PRE_DISCONNECT;
}

void status_led_notify_disconnect(uint8_t reason) {
    // 0x16 = local host terminated — inactivity path already showed 3× pre-blink.
    // No additional blink needed.
    if (reason == 0x16) {
        printf("[StatusLED] Disconnect reason=0x16 (local host) — no post-blink\n");
        return;
    }

    // If a PRE_DISCONNECT blink is somehow still running (race condition),
    // abort it cleanly before starting the post-disconnect blink.
    if (state == LedState::PRE_DISCONNECT) {
        printf("[StatusLED] PRE_DISCONNECT still active at HCI disconnect — aborting\n");
        set_gpio23(false);
    }

    // Only arm if IDLE (or we just cleared PRE_DISCONNECT above).
    // Do not interrupt a POST_DISCONNECT already in progress.
    if (state != LedState::IDLE && state != LedState::PRE_DISCONNECT) return;

    if (reason == 0x13) {
        // Remote user terminated — PS button hold
        target_half_cycles = POST_MANUAL_HALF_CYCLES;
        printf("[StatusLED] Disconnect reason=0x13 (manual) — 2× blink\n");
    } else {
        // Signal lost, timeout, error, or any other reason
        target_half_cycles = POST_ERROR_HALF_CYCLES;
        printf("[StatusLED] Disconnect reason=0x%02X (error/loss) — 4× blink\n", reason);
    }

    state          = LedState::POST_DISCONNECT;
    half_cycles    = 0;
    last_toggle_us = time_us_64();
    set_gpio23(true);
}
