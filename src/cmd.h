//
// Created by awalol on 2026/5/4.
// v1 — extended with profile commands
//

#ifndef DS5_BRIDGE_CMD_H
#define DS5_BRIDGE_CMD_H

#include <stdint.h>

// HID report IDs used for pico commands:
//
//   0xf6  SET  — write config / save / reconnect (original)
//   0xf7  GET  — read current Config_body (original)
//   0xf8  SET  — profile commands (new)
//   0xf9  GET  — profile query (new)

bool is_pico_cmd(uint8_t report_id);
uint16_t pico_cmd_get(uint8_t report_id, uint8_t *buffer, uint16_t reqlen);
void pico_cmd_set(uint8_t report_id, uint8_t const *buffer, uint16_t bufsize);

#endif // DS5_BRIDGE_CMD_H
