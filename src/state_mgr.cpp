//
// Robust State Manager for DS5Dongle (Waveshare RP2350B-Plus-W)
// Bridges game/DS4Windows states cleanly into high-frequency audio reports.
//

#include <cstring>
#include <cstdint>
#include <cstdio>
#include "state_mgr.h"

static constexpr uint8_t state_init_data[63] = {
    0xfd, 0xf7, 0x0, 0x0,
    0x7f, 0x64, // Headphones, Speaker volumes
    0xff, 0x9, 0x0, 0x0F, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xa,
    0x7, 0x0, 0x0, 0x2, 0x1,
    0x00,
    0xff, 0xd7, 0x00 // Default startup color (Orange)
};

uint8_t state[63]{};

void state_init() {
    std::memcpy(state, state_init_data, sizeof(state));
}

void state_set(uint8_t *data, const uint8_t size) {
    std::memcpy(data, state, size > 63 ? 63 : size);
}

void state_update(const uint8_t *data, const uint8_t size) {
    if (size < 48) return;

    // 1. Sync primary Sony hardware control flags
    state[0] = data[0]; // valid_flag0 (Rumble/Haptics)
    state[1] = data[1]; // valid_flag1 (Adaptive Triggers)
    state[2] = data[2]; // valid_flag2 (Lightbar/LEDs)

    // 2. FORCE DualSense Rumble Emulation DSP Compatibility
    // Tells the controller to translate motor bytes into haptic thumps 
    // while the high-speed 0x36 audio stream is running.
    if (data[0] & 0x01) { 
        state[0] |= 0x03;  // Force Rumble Emulation bits ON
        state[38] |= 0x04; // Force Improved Rumble Emulation bit ON
    }

    // 3. Sync standard rumble motor bytes
    state[3] = data[3]; // Right Motor Strength
    state[4] = data[4]; // Left Motor Strength

    // 4. Sync Adaptive Trigger Configurations (11 bytes each)
    if (data[1] & 0x01) std::memcpy(state + 11, data + 11, 11);
    if (data[1] & 0x02) std::memcpy(state + 22, data + 22, 11);

    // 5. Sync Microphone / Mute Button LED
    if (data[1] & 0x40) state[9] = data[9];

    // 6. Sync Lightbar Customizations (Fade, Brightness, Indicators)
    if (data[2] & 0x01) state[44] = data[44]; // Player Indicators
    if (data[2] & 0x04) state[42] = data[42]; // Light Fade
    if (data[2] & 0x08) state[43] = data[43]; // Light Brightness

    // 7. Sync DS4Windows Custom Lightbar Colors (RGB Channels)
    // If a custom color profile is present, force the activation flag ON
    if ((data[2] & 0x02) || data[45] != 0 || data[46] != 0 || data[47] != 0) {
        state[2] |= 0x02;     // Explicitly keep the LED modification flag alive
        state[45] = data[45]; // Red channel
        state[46] = data[46]; // Green channel
        state[47] = data[47]; // Blue channel
    }
}
