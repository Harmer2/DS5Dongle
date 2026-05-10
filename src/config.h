//
// Created by awalol on 2026/5/4.
// v1 — wear-levelling + multi-profile support
//

#ifndef DS5_BRIDGE_CONFIG_H
#define DS5_BRIDGE_CONFIG_H

#include <cstdint>

// Number of user profiles stored in flash.
// Each profile occupies one FLASH_SECTOR_SIZE (4KB).
// 8 profiles = 32KB total — negligible on a 16MB flash.
#define CONFIG_PROFILE_COUNT 8

struct __attribute__((packed)) Config_body {
    float haptics_gain;                  // [1.0, 2.0]
    float speaker_volume;                // [-100, 0]
    uint8_t inactive_time;               // [5, 60] min
    uint8_t disable_inactive_disconnect; // bool: 0=auto-disconnect, 1=keep alive
    uint8_t disable_pico_led;            // bool
    uint8_t polling_rate_mode;           // 0: 250Hz, 1: 500Hz, 2: real-time
    uint8_t audio_buffer_length;         // [16, 128]
    uint8_t controller_mode;             // 0: DS5, 1: DSE, 2: Auto
    uint8_t active_profile;              // [0, CONFIG_PROFILE_COUNT-1]
    uint8_t _pad[3];                     // alignment padding
};

struct __attribute__((packed)) Config {
    uint32_t magic;
    uint16_t version;
    uint32_t crc32;   // CRC of Config_body, calculated on save only
    uint16_t size;    // sizeof(Config_body) — version guard
    Config_body body;
};

// Wear-levelling slot: written into one 256-byte flash page.
// The ring sector holds 16 slots. On each save the next page is used.
// When all 16 are written, the sector is erased and the ring resets.
// This gives 16x more erase cycles before flash wear on the active config sector.
struct __attribute__((packed)) WL_Slot {
    uint32_t magic;     // must match CONFIG_MAGIC to be valid
    uint32_t sequence;  // monotonically increasing — highest is most recent
    Config   config;    // full config payload
};

void config_load();
bool config_save();
const Config_body& get_config();
void set_config(const uint8_t *new_config, uint16_t len);
void set_config(const Config_body &new_config);
void config_valid();

// Profile API
bool profile_load(uint8_t slot);   // load slot into RAM (does not auto-save)
bool profile_save(uint8_t slot);   // save current RAM config to slot
uint8_t profile_get_active();      // returns active profile index

extern bool is_dse;

#endif // DS5_BRIDGE_CONFIG_H
