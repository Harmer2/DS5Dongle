#include "config.h"
#include "utils.h"

#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

// Flash wear-leveling: use last 64KB (16 sectors × 4KB) of flash
// Cycle through sectors, newest valid entry wins
#define CONFIG_FLASH_BASE   (PICO_FLASH_SIZE_BYTES - (16 * FLASH_SECTOR_SIZE))
#define CONFIG_NUM_SECTORS  16
#define CONFIG_MAGIC        0x44533542  // "DS5B"

typedef struct {
    uint32_t magic;
    uint32_t sequence;  // Monotonically increasing — highest valid = current
    config_t config;
    uint32_t crc;       // CRC32 of magic + sequence + config
} config_entry_t;

static config_t current_config;
static uint32_t current_sequence = 0;
static int current_sector = 0;

// Default configuration — optimized for low latency gaming
static const config_t default_config = {
    .haptics_gain = 1.0f,
    .speaker_volume = 0.0f,         // 0 dB (unity gain)
    .audio_buffer_length = 24,      // 24ms — eliminates DualSense-side underruns
    .controller_mode = 2,           // Auto-detect DS/DSE
    .polling_rate = 1000,           // 1000Hz USB polling
    .disable_pico_led = 0,
    .inactive_time = 300,           // 5 minutes
    .disable_inactive_disconnect = 0,
};

static uint32_t config_crc(const config_entry_t* entry) {
    // CRC over everything except the CRC field itself
    return crc32((const uint8_t*)entry, offsetof(config_entry_t, crc));
}

void config_init(void) {
    // Scan all sectors, find highest valid sequence
    const uint8_t* flash_base = (const uint8_t*)(XIP_BASE + CONFIG_FLASH_BASE);
    bool found = false;

    for (int i = 0; i < CONFIG_NUM_SECTORS; i++) {
        const config_entry_t* entry = (const config_entry_t*)(flash_base + i * FLASH_SECTOR_SIZE);

        if (entry->magic != CONFIG_MAGIC) continue;

        uint32_t expected_crc = config_crc(entry);
        if (entry->crc != expected_crc) continue;

        // Valid entry — check if newest
        if (!found || entry->sequence > current_sequence) {
            current_config = entry->config;
            current_sequence = entry->sequence;
            current_sector = i;
            found = true;
        }
    }

    if (!found) {
        // No valid config — use defaults
        current_config = default_config;
        current_sequence = 0;
        current_sector = 0;
        // Write defaults to flash
        config_save();
    }
}

const config_t& get_config(void) {
    return current_config;
}

void set_config(const config_t& cfg) {
    current_config = cfg;
}

void config_save(void) {
    // Advance to next sector (wear-leveling round-robin)
    current_sector = (current_sector + 1) % CONFIG_NUM_SECTORS;
    current_sequence++;

    // Build entry
    config_entry_t entry;
    entry.magic = CONFIG_MAGIC;
    entry.sequence = current_sequence;
    entry.config = current_config;
    entry.crc = config_crc(&entry);

    // Flash offset (from start of flash, not XIP)
    uint32_t flash_offset = CONFIG_FLASH_BASE + current_sector * FLASH_SECTOR_SIZE;

    // CRITICAL: Must pause Core 1 during flash operations.
    // Flash erase/write disables XIP — if Core 1 is executing from flash, it crashes.
    // multicore_lockout pauses Core 1 safely in a RAM-resident spin loop.
    multicore_lockout_start_blocking();

    uint32_t ints = save_and_disable_interrupts();

    flash_range_erase(flash_offset, FLASH_SECTOR_SIZE);
    flash_range_program(flash_offset, (const uint8_t*)&entry, sizeof(config_entry_t));

    restore_interrupts(ints);

    multicore_lockout_end_blocking();
}

// Profile management stubs (not implemented yet)
uint8_t profile_get_active(void) {
    return 0; // Always return slot 0
}

bool profile_load(uint8_t slot) {
    (void)slot;
    return false; // Not implemented
}

bool profile_save(uint8_t slot) {
    (void)slot;
    return false; // Not implemented
}
