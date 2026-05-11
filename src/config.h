#ifndef DS5_BRIDGE_CONFIG_H
#define DS5_BRIDGE_CONFIG_H

#include <stdint.h>

#define CONFIG_PROFILE_COUNT 8

struct config_t {
    float haptics_gain;
    float speaker_volume;
    uint16_t audio_buffer_length;
    uint8_t controller_mode;
    uint16_t polling_rate;
    uint8_t disable_pico_led;
    uint16_t inactive_time;
    uint8_t disable_inactive_disconnect;
};

void config_init(void);
const config_t& get_config(void);
void set_config(const config_t& cfg);
void config_save(void);

uint8_t profile_get_active(void);
bool profile_load(uint8_t slot);
bool profile_save(uint8_t slot);

#endif // DS5_BRIDGE_CONFIG_H
