//
// Simplified State Manager for DS5Dongle (Waveshare RP2350B-Plus-W Fork)
// Bypasses the need for SetStateData struct by using direct byte mapping.
//

#include <cstddef>
#include <cstring>
#include <cstdio>
#include "state_mgr.h"

static constexpr uint8_t state_init_data[63] = {
    0xfd, 0xf7, 0x0, 0x0,
    0x7f, 0x64, // Headphones, Speaker
    0xff, 0x9, 0x0, 0x0F, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0xa,
    0x7, 0x0, 0x0, 0x2, 0x1,
    0x00,
    0xff, 0xd7, 0x00 // RGB LED: R, G, B (Nijika Color!)✨
};

uint8_t state[63]{};

void state_init() {
    memcpy(state, state_init_data, sizeof(state));
}

void state_set(uint8_t *data, const uint8_t size) {
    if (size > 63) {
        printf("[StateMgr] Warning: State Set over 63 bytes\n");
    }
    memcpy(data, state, size > 63 ? 63 : size);
}

void state_update(const uint8_t *data, const uint8_t size) {
    if (size < 48) {
        return;
    }

    auto set_bit = [](uint8_t &byte, const int bit, const bool value) {
        byte = (byte & ~(1 << bit)) | (value << bit);
    };

    // Apply the rumble update flags to byte 0
    bool enable_rumble = data[0] & 0x01;
    bool use_rumble_not_haptics = data[0] & 0x02;
    set_bit(state[0], 0, enable_rumble);
    set_bit(state[0], 1, use_rumble_not_haptics);
    
    // Improved rumble emulation flag on byte 38
    bool enable_improved_rumble = data[38] & 0x04;
    set_bit(state[38], 2, enable_improved_rumble);

    // Rumble Emulation (Right & Left motor)
    if (enable_rumble || use_rumble_not_haptics) {
        state[3] = data[3];
        state[4] = data[4];
    }

    // Mute Light Mode
    if (data[1] & 0x01) { 
        state[9] = data[9];
    }

    // Right Trigger FFB
    if (data[1] & 0x04) { 
        memcpy(state + 11, data + 11, 11);
    }

    // Left Trigger FFB
    if (data[1] & 0x08) { 
        memcpy(state + 22, data + 22, 11);
    }

    // Light Fade Animation
    if (data[2] & 0x01) { 
        state[42] = data[42];
    }

    // Light Brightness
    if (data[2] & 0x02) { 
        state[43] = data[43];
    }

    // Player Indicators
    if (data[2] & 0x04) { 
        state[44] = data[44];
    }

    // RGB LED
    if (data[2] & 0x08) { 
        state[45] = data[45]; // Red
        state[46] = data[46]; // Green
        state[47] = data[47]; // Blue
    }
}
