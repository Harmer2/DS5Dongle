//
// GPIO23 status LED driver — Waveshare RP2350B Plus W
//
// GPIO23 is a direct RP2350B GPIO (LED2 on the board).
// It is independent of the CYW43 driver and works from the first
// instruction of main(). Do not use cyw43_arch_gpio_put() for this LED.
//
// States:
//   BOOT_ON          — solid ON from power-up until status_led_cyw43_ready()
//   IDLE             — OFF
//   PRE_DISCONNECT   — blinks 3× at 4 Hz then calls bt_disconnect()
//                      (inactivity timeout warning)
//   POST_DISCONNECT  — blinks N× at 4 Hz after HCI disconnect event
//                      (reason-coded: 2× manual, 4× signal lost/error)
//                      does NOT call bt_disconnect()
//
// Call order:
//   1. status_led_init()               — call before board_init(), CYW43 not needed
//   2. status_led_cyw43_ready()        — call after cyw43_arch_init() succeeds
//   3. status_led_tick()               — call every main-loop iteration
//   4. status_led_warn_disconnect()    — call instead of bt_disconnect() for
//                                        inactivity timeout
//   5. status_led_notify_disconnect()  — call from HCI_EVENT_DISCONNECTION_COMPLETE
//                                        with the HCI reason byte
//

#pragma once

#include <cstdint>

// Initialise GPIO23 and turn it ON immediately (boot indicator).
// Safe to call before cyw43_arch_init().
void status_led_init(void);

// Turn GPIO23 OFF — call once after cyw43_arch_init() succeeds.
void status_led_cyw43_ready(void);

// Drive the blink state machine.
// Call every main-loop iteration. No-op when IDLE.
void status_led_tick(void);

// Arm the pre-disconnect warning (3× at 4 Hz, then calls bt_disconnect()).
// Call this instead of bt_disconnect() for inactivity timeout.
// Safe to call multiple times — re-arms only if currently IDLE.
void status_led_warn_disconnect(void);

// Returns true if a pre-disconnect warning is currently in progress.
// bt.cpp uses this to suppress re-arming while already blinking.
bool status_led_disconnect_pending(void);

// Called from HCI_EVENT_DISCONNECTION_COMPLETE with the HCI reason byte.
// Selects post-disconnect blink count based on reason:
//   0x13 — remote user terminated (PS button)  → 2× blink
//   0x16 — local host terminated (inactivity)  → no blink
//   any other reason                            → 4× blink
// Safe to call from BTstack callback context.
void status_led_notify_disconnect(uint8_t reason);
