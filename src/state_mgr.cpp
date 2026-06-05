//
// Official Sony Specification State Manager for DS5Dongle
// Maps USB payload layouts cleanly to Bluetooth cache layouts.
//

#include <cstring>
#include <cstdint>
#include <cstdio>
#include "state_mgr.h"
#include "utils.h"

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
    if (size < sizeof(SetStateData)) return;

    SetStateData update{};
    std::memcpy(&update, data, sizeof(update));

    auto set_bit = [](uint8_t &byte, const int bit, const bool value) {
        byte = (byte & ~(1 << bit)) | (value << bit);
    };
    const auto copy_if = [&](bool allowed, size_t offset, size_t length) {
        if (allowed) std::memcpy(state + offset, data + offset, length);
    };

    // Rumble emulation flag bits — merge with |= never overwrite
    set_bit(state[0], 0, update.EnableRumbleEmulation);
    set_bit(state[0], 1, update.UseRumbleNotHaptics);
    if (update.EnableImprovedRumbleEmulation) state[38] |= 0x04;

    // Rumble motor values
    copy_if(update.UseRumbleNotHaptics || update.EnableRumbleEmulation,
            offsetof(SetStateData, RumbleEmulationRight), 2);

    // Mute light
    copy_if(update.AllowMuteLight,
            offsetof(SetStateData, MuteLightMode), sizeof(update.MuteLightMode));

    // Adaptive trigger FFB
    copy_if(update.AllowRightTriggerFFB,
            offsetof(SetStateData, RightTriggerFFB), sizeof(update.RightTriggerFFB));
    copy_if(update.AllowLeftTriggerFFB,
            offsetof(SetStateData, LeftTriggerFFB), sizeof(update.LeftTriggerFFB));

    // LED fade animation + brightness
    copy_if(update.AllowColorLightFadeAnimation,
            offsetof(SetStateData, LightFadeAnimation), sizeof(update.LightFadeAnimation));
    copy_if(update.AllowLightBrightnessChange,
            offsetof(SetStateData, LightBrightness), sizeof(update.LightBrightness));

    // Player indicator LEDs (byte immediately before LedRed)
    copy_if(update.AllowPlayerIndicators,
            offsetof(SetStateData, LedRed) - 1, sizeof(uint8_t));

    // RGB LED color — only written when AllowLedColor is set,
    // so rumble-only packets can never wipe the color
    copy_if(update.AllowLedColor,
            offsetof(SetStateData, LedRed), 3);
}

// Keep signature alive for header compilation safety
void state_update_from_game(const uint8_t *data, uint16_t size) {
    state_update(data, static_cast<uint8_t>(size));
}
