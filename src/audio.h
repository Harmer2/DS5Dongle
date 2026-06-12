#ifndef DS5_BRIDGE_AUDIO_H
#define DS5_BRIDGE_AUDIO_H

#include <cstdint>

#ifndef ENABLE_AUDIO
#define ENABLE_AUDIO 1
#endif

#if ENABLE_AUDIO
void audio_init();
void audio_loop();
void core1_entry();
void set_headset(bool state);
void set_state_data(const uint8_t* data, const uint8_t len);
#else
inline void audio_init() {}
inline void audio_loop() {}
inline void set_headset(bool) {}
inline void set_state_data(const uint8_t*, uint8_t) {}
#endif

#endif //DS5_BRIDGE_AUDIO_H
