//
// GPIO23 status LED driver — Waveshare RP2350B Plus W
//
// GPIO23 is a direct RP2350B GPIO (LED2 on the board).
// It is independent of the CYW43 driver and works from the first
// instruction of main(). Do not use cyw43_arch_gpio_put() for this LED.
//
// States:
//   BOOT_ON        — solid ON from power-up until status_led_cyw43_ready()
//   IDLE           — OFF
//   PRE_DISCONNECT — blinks 4 Hz for 1.5 s then calls bt_disconnect()
//
// Call order:
//   1. status_led_init()          — call before board_init(), CYW43 not needed
//   2. status_led_cyw43_ready()   — call after cyw43_arch_init() succeeds
//   3. status_led_tick()          — call every main-loop iteration
//   4. status_led_warn_disconnect() — call instead of bt_disconnect() for
//                                     inactivity timeout
//

#pragma once

// Initialise GPIO23 and turn it ON immediately (boot indicator).
// Safe to call before cyw43_arch_init().
void status_led_init(void);

// Turn GPIO23 OFF — call once after cyw43_arch_init() succeeds.
void status_led_cyw43_ready(void);

// Drive the disconnect-warning blink state machine.
// Call every main-loop iteration. No-op when IDLE.
void status_led_tick(void);

// Arm the pre-disconnect warning (3 × 4 Hz blinks, then bt_disconnect()).
// Call this instead of bt_disconnect() for inactivity timeout.
// Safe to call multiple times — re-arms only if currently IDLE.
void status_led_warn_disconnect(void);

// Returns true if a disconnect warning is currently in progress.
// bt.cpp uses this to suppress re-arming while already blinking.
bool status_led_disconnect_pending(void);
