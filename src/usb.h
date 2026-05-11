#ifndef DS5_BRIDGE_USB_H
#define DS5_BRIDGE_USB_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void usb_init(void);
void usb_task(void);
void usb_send_hid_report(const uint8_t *report, uint16_t len);
bool usb_mounted(void);

#ifdef __cplusplus
}
#endif

#endif // DS5_BRIDGE_USB_H
