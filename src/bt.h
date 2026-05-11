#ifndef BT_H
#define BT_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void bt_init(void);
void bt_poll(void);
bool bt_is_connected(void);

// Send output report (audio+haptics) to DualSense via L2CAP interrupt channel
// Uses drop-oldest FIFO: always sends freshest data, never blocks
void bt_write(const uint8_t* data, uint16_t len);

// Send feature report to DualSense via L2CAP control channel
void bt_send_feature_report(const uint8_t* data, uint16_t len);

// Get last received feature report response (for USB GET_REPORT proxy)
const uint8_t* bt_get_feature_report(uint16_t* out_len);

#ifdef __cplusplus
}
#endif

#endif // BT_H
