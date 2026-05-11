// Created by awalol on 2026/5/4.
// v2-opt — default audio_buffer_length 64->16, polling_rate_mode 0->2
//
// Flash layout (top of 16MB flash, working downward):
//
//   Offset from end      Size    Purpose
//   ──────────────────────────────────────────────────────
//   -4KB  (1 sector)     4KB     Active config wear-level ring
//                                16 x 256-byte WL_Slot pages.
//                                Each save writes to the next page.
//                                Sector erased only when ring wraps (every 16 saves).
//                                Gives 16x more writes before erase wear.
//
//   -8KB  (1 sector)     4KB     Profile slot 0
//   -12KB (1 sector)     4KB     Profile slot 1
//   ...
//   -40KB (1 sector)     4KB     Profile slot 7
//
//   Total used: 9 sectors = 36KB out of 16384KB.
//

#include "config.h"
#include <cmath>
#include <cstring>
#include "utils.h"
#include "hardware/flash.h"
#include "hardware/sync.h"
#include "pico/cyw43_arch.h"

// ── Constants ────────────────────────────────────────────────────────────────

constexpr uint32_t CONFIG_MAGIC   = 0x66ccff00;
constexpr uint16_t CONFIG_VERSION = 2;

// 16 pages per sector (4096 / 256)
constexpr uint32_t WL_PAGES = FLASH_SECTOR_SIZE / FLASH_PAGE_SIZE;

// Wear-level ring: last sector of flash
constexpr uint32_t WL_FLASH_OFFSET = PICO_FLASH_SIZE_BYTES - FLASH_SECTOR_SIZE;

// Profile sectors: immediately below the WL ring, one sector each
static constexpr uint32_t profile_offset(uint8_t slot) {
    return WL_FLASH_OFFSET - (static_cast<uint32_t>(slot + 1) * FLASH_SECTOR_SIZE);
}

// ── Compile-time guards ───────────────────────────────────────────────────────

static_assert(sizeof(WL_Slot) <= FLASH_PAGE_SIZE,
    "WL_Slot must fit in one flash page (256 bytes)");
static_assert(sizeof(Config) <= FLASH_PAGE_SIZE,
    "Config must fit in one flash page (256 bytes)");
static_assert(WL_FLASH_OFFSET % FLASH_SECTOR_SIZE == 0,
    "WL sector must be sector-aligned");

// ── State ─────────────────────────────────────────────────────────────────────

static Config config{};
bool is_dse = false;

// ── CRC ──────────────────────────────────────────────────────────────────────

static uint32_t calc_config_crc(const Config &c) {
    return crc32(reinterpret_cast<const uint8_t *>(&c.body), sizeof(Config_body));
}

// ── Validation ───────────────────────────────────────────────────────────────

void config_valid() {
    if (config.magic != CONFIG_MAGIC) {
        config.magic = CONFIG_MAGIC;
        printf("[Config] Magic invalid — reset\n");
    }
    if (config.version != CONFIG_VERSION) {
        config.version = CONFIG_VERSION;
        printf("[Config] Version invalid — reset\n");
    }
    if (config.size != sizeof(Config_body)) {
        config.size = sizeof(Config_body);
        printf("[Config] Body size invalid — reset\n");
    }

    auto *b = &config.body;

    if (std::isnan(b->haptics_gain) || b->haptics_gain < 1.0f || b->haptics_gain > 2.0f) {
        b->haptics_gain = 1.0f;
        printf("[Config] haptics_gain invalid — reset\n");
    }
    if (std::isnan(b->speaker_volume) || b->speaker_volume < -100.0f || b->speaker_volume > 0.0f) {
        b->speaker_volume = -100.0f;
        printf("[Config] speaker_volume invalid — reset\n");
    }
    if (b->inactive_time < 5 || b->inactive_time > 60) {
        b->inactive_time = 30;
        printf("[Config] inactive_time invalid — reset\n");
    }
    if (b->disable_inactive_disconnect > 1) {
        b->disable_inactive_disconnect = 0;
        printf("[Config] disable_inactive_disconnect invalid — reset\n");
    }
    if (b->disable_pico_led > 1) {
        b->disable_pico_led = 0;
        printf("[Config] disable_pico_led invalid — reset\n");
    }
    if (b->polling_rate_mode > 2) {
        // FIX: default to real-time mode (2) instead of 250Hz (0).
        // Mode 2 sends input reports immediately when BT data arrives,
        // eliminating the fixed 4ms polling delay that caused input lag.
        b->polling_rate_mode = 2;
        printf("[Config] polling_rate_mode invalid — reset to real-time\n");
    }
    if (b->audio_buffer_length < 16 || b->audio_buffer_length > 128) {
        // FIX: default 64->16. Lower value = less audio delay on the DualSense.
        // 64 told the controller to buffer ~64 frames before playing,
        // which was the primary cause of the audio lag you heard.
        // 16 is the minimum allowed; increase to 24-32 if you get dropouts.
        b->audio_buffer_length = 16;
        printf("[Config] audio_buffer_length invalid — reset to 16\n");
    }
    if (b->controller_mode > 2) {
        b->controller_mode = 2;
        printf("[Config] controller_mode invalid — reset\n");
    }
    if (b->active_profile >= CONFIG_PROFILE_COUNT) {
        b->active_profile = 0;
        printf("[Config] active_profile invalid — reset\n");
    }
}

// ── Wear-level ring ───────────────────────────────────────────────────────────

static const WL_Slot *wl_slot_ptr(uint32_t i) {
    return reinterpret_cast<const WL_Slot *>(
        XIP_BASE + WL_FLASH_OFFSET + i * FLASH_PAGE_SIZE);
}

// Returns index of most recent valid slot, or WL_PAGES if ring is empty.
static uint32_t wl_find_latest() {
    uint32_t best_idx = WL_PAGES;
    uint32_t best_seq = 0;
    for (uint32_t i = 0; i < WL_PAGES; i++) {
        const WL_Slot *s = wl_slot_ptr(i);
        if (s->magic != CONFIG_MAGIC) continue;
        if (s->sequence >= best_seq) {
            best_seq = s->sequence;
            best_idx = i;
        }
    }
    return best_idx;
}

static bool wl_write(const Config &c) {
    uint32_t latest    = wl_find_latest();
    uint32_t next_seq  = 1;
    uint32_t next_page = 0;
    bool     need_erase = false;

    if (latest == WL_PAGES) {
        need_erase = true;
    } else {
        next_seq  = wl_slot_ptr(latest)->sequence + 1;
        next_page = (latest + 1) % WL_PAGES;
        if (next_page == 0) need_erase = true;
    }

    alignas(4) uint8_t page_buf[FLASH_PAGE_SIZE];
    memset(page_buf, 0xff, sizeof(page_buf));

    WL_Slot slot{};
    slot.magic    = CONFIG_MAGIC;
    slot.sequence = next_seq;
    slot.config   = c;
    memcpy(page_buf, &slot, sizeof(WL_Slot));

    const uint32_t ints = save_and_disable_interrupts();
    if (need_erase) {
        flash_range_erase(WL_FLASH_OFFSET, FLASH_SECTOR_SIZE);
    }
    flash_range_program(WL_FLASH_OFFSET + next_page * FLASH_PAGE_SIZE,
                        page_buf, FLASH_PAGE_SIZE);
    restore_interrupts(ints);

    const WL_Slot *written = wl_slot_ptr(next_page);
    return (written->magic    == CONFIG_MAGIC &&
            written->sequence == next_seq &&
            calc_config_crc(written->config) == calc_config_crc(c));
}

// ── Public config API ─────────────────────────────────────────────────────────

void config_load() {
    uint32_t latest = wl_find_latest();
    if (latest == WL_PAGES) {
        printf("[Config] No valid slot found — using defaults\n");
        config = Config{};
    } else {
        config = wl_slot_ptr(latest)->config;
        printf("[Config] Loaded from WL slot %lu (seq=%lu)\n",
               latest, wl_slot_ptr(latest)->sequence);
    }
    config_valid();
}

bool config_save() {
    config.magic   = CONFIG_MAGIC;
    config.version = CONFIG_VERSION;
    config.size    = sizeof(Config_body);
    config.crc32   = calc_config_crc(config);

    bool ok = wl_write(config);
    printf("[Config] Save %s\n", ok ? "OK" : "FAILED");
    return ok;
}

const Config_body &get_config() {
    return config.body;
}

void set_config(const uint8_t *new_config, uint16_t len) {
    const uint16_t copy_len = len < sizeof(Config_body) ? len : sizeof(Config_body);
    memcpy(&config.body, new_config, copy_len);
    config_valid();
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, !config.body.disable_pico_led);
}

void set_config(const Config_body &new_config) {
    config.body = new_config;
    config_valid();
}

// ── Profile API ───────────────────────────────────────────────────────────────

static const Config *profile_flash_ptr(uint8_t slot) {
    return reinterpret_cast<const Config *>(XIP_BASE + profile_offset(slot));
}

bool profile_load(uint8_t slot) {
    if (slot >= CONFIG_PROFILE_COUNT) {
        printf("[Profile] Invalid slot %u\n", slot);
        return false;
    }
    const Config *p = profile_flash_ptr(slot);
    if (p->magic != CONFIG_MAGIC) {
        printf("[Profile] Slot %u empty\n", slot);
        return false;
    }
    if (calc_config_crc(*p) != p->crc32) {
        printf("[Profile] Slot %u CRC mismatch\n", slot);
        return false;
    }
    config = *p;
    config.body.active_profile = slot;
    config_valid();
    printf("[Profile] Loaded slot %u\n", slot);
    return true;
}

bool profile_save(uint8_t slot) {
    if (slot >= CONFIG_PROFILE_COUNT) {
        printf("[Profile] Invalid slot %u\n", slot);
        return false;
    }

    Config to_save = config;
    to_save.magic   = CONFIG_MAGIC;
    to_save.version = CONFIG_VERSION;
    to_save.size    = sizeof(Config_body);
    to_save.body.active_profile = slot;
    to_save.crc32   = calc_config_crc(to_save);

    alignas(4) uint8_t page_buf[FLASH_PAGE_SIZE];
    memset(page_buf, 0xff, sizeof(page_buf));
    memcpy(page_buf, &to_save, sizeof(Config));

    const uint32_t offset = profile_offset(slot);
    const uint32_t ints   = save_and_disable_interrupts();
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
    flash_range_program(offset, page_buf, FLASH_PAGE_SIZE);
    restore_interrupts(ints);

    const Config *written = profile_flash_ptr(slot);
    bool ok = (written->magic == CONFIG_MAGIC &&
               calc_config_crc(*written) == to_save.crc32);
    printf("[Profile] Save slot %u: %s\n", slot, ok ? "OK" : "FAILED");
    return ok;
}

uint8_t profile_get_active() {
    return config.body.active_profile;
}
