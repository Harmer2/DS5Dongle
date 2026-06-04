//
// Official Sony Specification State Manager for DS5Dongle
// Maps USB payload layouts cleanly to Bluetooth cache layouts.
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
    0xff, 0xd7, 0x00 // Default Orange Layout
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

    // 1. Copy the raw USB payload into our Bluetooth cache buffer
    uint8_t copy_size = size > 63 ? 63 : size;
    std::memcpy(state, data, copy_size);

    // 2. FIX THE FLAG MAPPING MISMATCH:
    // On USB, valid_flag2 is at byte 2. On Bluetooth, it MUST live at byte 38.
    uint8_t usb_valid_flag2 = data[2];
    state[38] = usb_valid_flag2; 
    state[2] = 0; // Clear byte 2 since it's reserved padding on Bluetooth

    // 3. FORCE RUMBLE EMULATION COMPATIBILITY:
    // When the audio loop sends 0x36 haptic packets, standard rumble is ignored 
    // unless the DualSense's internal Rumble Emulation DSP is explicitly turned ON.
    if (data[0] & 0x01) { 
        state[0] |= 0x03;   // Force EnableRumbleEmulation & UseRumbleNotHaptics bits ON
        state[38] |= 0x04;  // Force EnableImprovedRumbleEmulation bit ON
    }
}

// Keep signature alive for header compilation safety
void state_update_from_game(const uint8_t *data, uint16_t size) {
    state_update(data, static_cast<uint8_t>(size));
}
