// REPLACE THE ENTIRE FILE WITH:
#ifndef DS5_BRIDGE_AUDIO_H
#define DS5_BRIDGE_AUDIO_H

#include 

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
// Inline stubs — callers compile cleanly with no #ifdef at every call site
inline void audio_init() {}
inline void audio_loop() {}
inline void set_headset(bool) {}
inline void set_state_data(const uint8_t*, uint8_t) {}
#endif

#endif //DS5_BRIDGE_AUDIO_H
