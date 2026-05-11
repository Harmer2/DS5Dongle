#include "usb.h"
#include "config.h"
#include "cmd.h"
#include "audio.h"
#include "tusb.h"

#include <cstring>

// DualSense USB input report size (report 0x01)
static constexpr uint16_t DS_INPUT_REPORT_USB_SIZE = 64;

// HID report buffer for forwarding from BT → USB host
static uint8_t hid_report_buf[DS_INPUT_REPORT_USB_SIZE];
static volatile bool hid_report_ready = false;

void usb_init(void) {
    tusb_init();
}

void usb_task(void) {
    tud_task();

    // If we have a pending HID report and the endpoint is ready, send it
    if (hid_report_ready && tud_hid_ready()) {
        tud_hid_report(0x01, hid_report_buf, DS_INPUT_REPORT_USB_SIZE);
        hid_report_ready = false;
    }
}

void usb_send_hid_report(const uint8_t *report, uint16_t len) {
    if (len > DS_INPUT_REPORT_USB_SIZE) {
        len = DS_INPUT_REPORT_USB_SIZE;
    }
    memcpy(hid_report_buf, report, len);
    if (len < DS_INPUT_REPORT_USB_SIZE) {
        memset(hid_report_buf + len, 0, DS_INPUT_REPORT_USB_SIZE - len);
    }
    hid_report_ready = true;
}

bool usb_mounted(void) {
    return tud_mounted();
}

// --- TinyUSB HID Callbacks (weak symbol overrides) ---

extern "C" uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                            hid_report_type_t report_type,
                                            uint8_t *buffer, uint16_t reqlen) {
    (void)instance;

    if (report_type == HID_REPORT_TYPE_FEATURE) {
        return pico_cmd_get(report_id, buffer, reqlen);
    }

    if (report_type == HID_REPORT_TYPE_INPUT && report_id == 0x01) {
        uint16_t copy_len = (reqlen < DS_INPUT_REPORT_USB_SIZE) ? reqlen : DS_INPUT_REPORT_USB_SIZE;
        memcpy(buffer, hid_report_buf, copy_len);
        return copy_len;
    }

    return 0;
}

extern "C" void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                                       hid_report_type_t report_type,
                                       uint8_t const *buffer, uint16_t bufsize) {
    (void)instance;

    if (report_type == HID_REPORT_TYPE_FEATURE) {
        pico_cmd_set(report_id, buffer, bufsize);
        return;
    }

    if (report_type == HID_REPORT_TYPE_OUTPUT && report_id == 0x02) {
        set_state_data(buffer, bufsize);
        return;
    }
}
