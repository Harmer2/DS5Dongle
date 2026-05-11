//
// Created by awalol on 2026/5/4.
// v1 — extended with profile commands
//
// Command protocol:
//
//   GET 0xf7                          → returns current Config_body (original)
//
//   SET 0xf6, buf[0]=0x01             → set config in RAM
//   SET 0xf6, buf[0]=0x02             → save config to flash (wear-level ring)
//   SET 0xf6, buf[0]=0x03             → reconnect TinyUSB device
//
//   SET 0xf8, buf[0]=0x01, buf[1]=N   → load profile slot N [0-7] into RAM
//   SET 0xf8, buf[0]=0x02, buf[1]=N   → save current RAM config to profile slot N
//   SET 0xf8, buf[0]=0x03, buf[1]=N   → atomic switch: load slot N + save as active
//
//   GET 0xf9                          → returns [active_slot, CONFIG_PROFILE_COUNT]
//

#include "cmd.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include "config.h"
#include "device/usbd.h"
#include "pico/time.h"

bool is_pico_cmd(uint8_t report_id) {
    return (report_id == 0xf6 ||
            report_id == 0xf7 ||
            report_id == 0xf8 ||
            report_id == 0xf9);
}

uint16_t pico_cmd_get(uint8_t report_id, uint8_t *buffer, uint16_t reqlen) {
    // Original: GET current config body
    if (report_id == 0xf7) {
        printf("[HID] 0xf7 GET config\n");
     if (sizeof(config_t) > reqlen) {
    printf("[Config] Warning: config_t larger than reqlen\n");
}
const auto len = std::min(sizeof(config_t), static_cast<size_t>(reqlen));
        memcpy(buffer, &get_config(), len);
        return static_cast<uint16_t>(len);
    }

    // New: GET profile status → [active_profile, CONFIG_PROFILE_COUNT]
    if (report_id == 0xf9) {
        printf("[HID] 0xf9 GET profile status\n");
        if (reqlen < 2) return 0;
        buffer[0] = profile_get_active();
        buffer[1] = CONFIG_PROFILE_COUNT;
        return 2;
    }

    return 0;
}

void pico_cmd_set(uint8_t report_id, uint8_t const *buffer, uint16_t bufsize) {
    if (bufsize == 0) return;

    // Original: config set / save / reconnect
    if (report_id == 0xf6) {
        if (buffer[0] == 0x01) {
            printf("[CMD] 0xf6/0x01 config set\n");
            if (bufsize >= sizeof(config_t) + 1) {
    config_t cfg;
    memcpy(&cfg, buffer + 1, sizeof(config_t));
    set_config(cfg);
}
        }
        if (buffer[0] == 0x02) {
            printf("[CMD] 0xf6/0x02 config save\n");
            config_save();
        }
        if (buffer[0] == 0x03) {
            printf("[CMD] 0xf6/0x03 tud reconnect\n");
            tud_disconnect();
            sleep_ms(150);
            tud_connect();
        }
        return;
    }

    // New: profile commands
    if (report_id == 0xf8) {
        if (bufsize < 2) {
            printf("[CMD] 0xf8 missing slot byte\n");
            return;
        }
        const uint8_t slot = buffer[1];

        // Load profile slot into RAM (does not save active config first)
        if (buffer[0] == 0x01) {
            printf("[CMD] 0xf8/0x01 profile load slot=%u\n", slot);
            if (!profile_load(slot)) {
                printf("[CMD] Profile load slot %u failed\n", slot);
            }
        }

        // Save current RAM config to profile slot
        if (buffer[0] == 0x02) {
            printf("[CMD] 0xf8/0x02 profile save slot=%u\n", slot);
            if (!profile_save(slot)) {
                printf("[CMD] Profile save slot %u failed\n", slot);
            }
        }

        // Atomic switch: load profile AND save as active config in one command
        if (buffer[0] == 0x03) {
            printf("[CMD] 0xf8/0x03 profile switch slot=%u\n", slot);
            if (profile_load(slot)) {
                if (!config_save()) {
                    printf("[CMD] Profile switch: active config save failed\n");
                }
                #define CONFIG_PROFILE_COUNT 8

uint8_t profile_get_active(void);
bool profile_load(uint8_t slot);
bool profile_save(uint8_t slot);

            } else {
                printf("[CMD] Profile switch: load slot %u failed\n", slot);
            }
        }

        return;
    }
}
