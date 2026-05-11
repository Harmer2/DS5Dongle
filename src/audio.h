#ifndef DS5_BRIDGE_AUDIO_H
#define DS5_BRIDGE_AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void audio_init(void);
void audio_loop(void);
void audio_update_gains(void);

// Set DualSense output state (LED/rumble/triggers) — constructs BT report 0x31
// Called from usb.cpp when host sends output report 0x02
void set_state_data(const uint8_t *data, uint16_t len);

// Toggle headset-connected flag in DualSense output reports
void set_headset(bool connected);

#ifdef __cplusplus
}
#endif

#endif // DS5_BRIDGE_AUDIO_H
