//
// Created by awalol on 2026/5/15.
// Fully restored structure layout for older forks.
//

#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdint>

#include "utils.h"
#include "state_mgr.h"

// Define the official packed Sony DualSense Output Report structure layout
#pragma pack(push, 1)
struct SetStateData {
    // Byte 0
    uint8_t EnableRumbleEmulation : 1;
    uint8_t UseRumbleNotHaptics : 1;
    uint8_t AllowHeadphoneVolume : 1;
    uint8_t AllowSpeakerVolume : 1;
    uint8_t AllowMicVolume : 1;
    uint8_t AllowAudioControl : 1;
    uint8_t AllowMuteLight : 1;
    uint8_t AllowAudioMute : 1;

    // Byte 1
    uint8_t AllowRightTriggerFFB : 1;
    uint8_t AllowLeftTriggerFFB : 1;
    uint8_t AllowHeadphoneBalance : 1;
    uint8_t AllowAudioControl2 : 1;
    uint8_t AllowHapticLowPassFilter : 1;
    uint8_t AllowMotorPowerLevel : 1;
    uint8_t AllowColorLightFadeAnimation : 1;
    uint8_t AllowLightBrightnessChange : 1;

    // Byte 2
    uint8_t AllowPlayerIndicators : 1;
    uint8_t AllowLedColor : 1;
    uint8_t EnableImprovedRumbleEmulation : 1;
    uint8_t padding_flags : 5;

    // Byte 3
    uint8_t RumbleEmulationRight;
    uint8_t RumbleEmulationLeft;

    // Byte 5
    uint8_t VolumeHeadphones;
    uint8_t VolumeSpeaker;
    uint8_t VolumeMic;
    uint8_t AudioControl;
    uint8_t MuteLightMode;
    uint8_t AudioMute;

    // Byte 11
    uint8_t RightTriggerFFB[11];
    // Byte 22
    uint8_t LeftTriggerFFB[11];

    // Byte 33
    uint8_t HeadphoneBalance;
    uint8_t AudioControl2;

    // Byte 35
    uint32_t HostTimestamp;

    // Byte 39
    uint8_t MotorPowerLevel;
    // Byte 40
    uint8_t HapticLowPassFilter;
    // Byte 41
    uint8_t ReservedPadding;

    // Byte 42
    uint8_t LightFadeAnimation;
    // Byte 43
    uint8_t LightBrightness;
    // Byte 44
    uint8_t PlayerIndicators;

    // Byte 45
    uint8_t LedRed;
    uint8_t LedGreen;
    uint8_t LedBlue;
};
#pragma pack(pop)

namespace {
    constexpr size_t kAudioControlOffset = offsetof(SetStateData, MuteLightMode) - sizeof(uint8_t);
    constexpr size_t kMuteControlOffset = offsetof(SetStateData, RightTriggerFFB) - sizeof(uint8_t);
    constexpr size_t kMotorPowerLevelOffset = offsetof(SetStateData, HostTimestamp) + sizeof(uint32_t);
    constexpr size_t kAudioControl2Offset = kMotorPowerLevelOffset + sizeof(uint8_t);
    constexpr size_t kHapticLowPassFilterOffset = offsetof(SetStateData, LightFadeAnimation) - 2 * sizeof(uint8_t);
    constexpr size_t kPlayerIndicatorsOffset = offsetof(SetStateData, LedRed) - sizeof(uint8_t);
}

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
    if (size < sizeof(SetStateData)) {
        printf(
            "[StateMgr] Error: SetStateData at least %u bytes\n",
            static_cast<unsigned>(sizeof(SetStateData))
        );
        return;
    }

    SetStateData update{};
    memcpy(&update, data, sizeof(update));

    const auto copy_if_allowed = [&](const bool allowed, const size_t offset, const size_t length) {
        if (allowed) {
            memcpy(state + offset, data + offset, length);
        }
    };
    auto set_bit = [](uint8_t &byte, const int bit, const bool value) {
        byte = (byte & ~(1 << bit)) | (value << bit);
    };

    set_bit(state[0], 0, update.EnableRumbleEmulation);
    set_bit(state[0], 1, update.UseRumbleNotHaptics);
    set_bit(state[38], 2, update.EnableImprovedRumbleEmulation);
    copy_if_allowed(
        update.UseRumbleNotHaptics || update.EnableRumbleEmulation,
        offsetof(SetStateData, RumbleEmulationRight),
        2
    );

    /*copy_if_allowed(
        update.AllowHeadphoneVolume,
        offsetof(SetStateData, VolumeHeadphones),
        sizeof(update.VolumeHeadphones)
    );*/
    /*copy_if_allowed(
        update.AllowSpeakerVolume,
        offsetof(SetStateData, VolumeSpeaker),
        sizeof(update.VolumeSpeaker)
    );*/
    /*copy_if_allowed(
        update.AllowMicVolume,
        offsetof(SetStateData, VolumeMic),
        sizeof(update.VolumeMic)
    );*/
    /*copy_if_allowed(
        update.AllowAudioControl,
        kAudioControlOffset,
        sizeof(uint8_t)
    );*/

    copy_if_allowed(
        update.AllowMuteLight,
        offsetof(SetStateData, MuteLightMode),
        sizeof(update.MuteLightMode)
    );

    /*copy_if_allowed(
        update.AllowAudioMute,
        kMuteControlOffset,
        sizeof(uint8_t)
    );*/

    copy_if_allowed(
        update.AllowRightTriggerFFB,
        offsetof(SetStateData, RightTriggerFFB),
        sizeof(update.RightTriggerFFB)
    );
    copy_if_allowed(
        update.AllowLeftTriggerFFB,
        offsetof(SetStateData, LeftTriggerFFB),
        sizeof(update.LeftTriggerFFB)
    );

    /*copy_if_allowed(
        update.AllowMotorPowerLevel,
        kMotorPowerLevelOffset,
        sizeof(uint8_t)
    );*/
    /*copy_if_allowed(
        update.AllowAudioControl2,
        kAudioControl2Offset,
        sizeof(uint8_t)
    );*/
    /*copy_if_allowed(
        update.AllowHapticLowPassFilter,
        kHapticLowPassFilterOffset,
        sizeof(uint8_t)
    );*/

    copy_if_allowed(
        update.AllowColorLightFadeAnimation,
        offsetof(SetStateData, LightFadeAnimation),
        sizeof(update.LightFadeAnimation)
    );
    copy_if_allowed(
        update.AllowLightBrightnessChange,
        offsetof(SetStateData, LightBrightness),
        sizeof(update.LightBrightness)
    );
    copy_if_allowed(
        update.AllowPlayerIndicators,
        kPlayerIndicatorsOffset,
        sizeof(uint8_t)
    );
    copy_if_allowed(
        update.AllowLedColor,
        offsetof(SetStateData, LedRed),
        sizeof(update.LedRed) * 3
    );
}
