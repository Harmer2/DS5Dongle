//
// Created by awalol on 2026/3/4.
// v2 — 360 MHz + 1.25V, Waveshare RP2350B-Plus-W
// v2-batt — added ENABLE_BATT_LED support
// v3 — GPIO23 boot/disconnect LED via status_led
//

#include <cstdio>
#include "bsp/board_api.h"
#include "bt.h"
#include "utils.h"
#include "resample.h"
#include "audio.h"
#include "hardware/clocks.h"
#include "hardware/vreg.h"
#include "hardware/watchdog.h"
#include "pico/cyw43_arch.h"
#include "status_led.h"
#if ENABLE_SERIAL
#include "pico/stdio_usb.h"
#endif
#include "config.h"
#include "cmd.h"
#include "pico/critical_section.h"

#if ENABLE_BATT_LED
#include "battery_led.h"
#endif

// battery_led.cpp reads interrupt_in_data[52] directly via extern.
// set_state_data lives in audio.cpp.
extern void set_state_data(const uint8_t* data, const uint8_t len);

int reportSeqCounter = 0;
uint8_t packetCounter = 0;
bool spk_active = false;

uint8_t interrupt_in_data[63] = {
    0x7f, 0x7d, 0x7f, 0x7e, 0x00, 0x00, 0xa7,
    0x08, 0x00, 0x00, 0x00, 0x52, 0x43, 0x30, 0x41,
    0x01, 0x00, 0x0e, 0x00, 0xef, 0xff, 0x03, 0x03,
    0x7b, 0x1b, 0x18, 0xf0, 0xcc, 0x9c, 0x60, 0x00,
    0xfc, 0x80, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00,
    0x00, 0x00, 0x09, 0x09, 0x00, 0x00, 0x00, 0x00,
    0x00, 0xa7, 0xad, 0x60, 0x00, 0x29, 0x18, 0x00,
    0x53, 0x9f, 0x28, 0x35, 0xa5, 0xa8, 0x0c, 0x8b
};

critical_section_t report_cs;
volatile bool report_dirty = false;

void interrupt_loop() {
    if (!tud_hid_ready()) return;

    if (get_config().polling_rate_mode != 2) {
        if (!tud_hid_report(0x01, interrupt_in_data, 63)) {
            printf("[USBHID] tud_hid_report error\n");
        }
        return;
    }

    bool should_send = false;
    uint8_t safe_report[63];

    critical_section_enter_blocking(&report_cs);
    if (report_dirty) {
        memcpy(safe_report, interrupt_in_data, 63);
        report_dirty = false;
        should_send = true;
    }
    critical_section_exit(&report_cs);

    if (should_send) {
        if (!tud_hid_report(0x01, safe_report, 63)) {
            printf("[USBHID] tud_hid_report error\n");
            critical_section_enter_blocking(&report_cs);
            report_dirty = true;
            critical_section_exit(&report_cs);
        }
    }
}

void on_bt_data(CHANNEL_TYPE channel, uint8_t *data, uint16_t len) {
    if (channel == INTERRUPT && data[1] == 0x31) {
        if ((data[56] & 1) != (interrupt_in_data[53] & 1)) {
            set_headset(data[56] & 1);
        }

        if (get_config().polling_rate_mode != 2) {
            memcpy(interrupt_in_data, data + 3, 63);
#if ENABLE_BATT_LED
            battery_led_note_report();
#endif
            return;
        }

        critical_section_enter_blocking(&report_cs);
        memcpy(interrupt_in_data, data + 3, 63);
        report_dirty = true;
        critical_section_exit(&report_cs);

#if ENABLE_BATT_LED
        battery_led_note_report();
#endif
    }
}

uint16_t tud_hid_get_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer,
                               uint16_t reqlen) {
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) reqlen;

    if (is_pico_cmd(report_id)) {
        return pico_cmd_get(report_id, buffer, reqlen);
    }

    std::vector<uint8_t> feature_data = get_feature_data(report_id, reqlen);
    if (!feature_data.empty()) {
        memcpy(buffer, feature_data.data() + 1, feature_data.size() - 1);
    }

    return feature_data.empty() ? 0 : feature_data.size() - 1;
}

bool tud_audio_set_itf_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void) rhport;
    uint8_t const itf = tu_u16_low(p_request->wIndex);
    uint8_t const alt = tu_u16_low(p_request->wValue);

    if (itf == 1) {
        printf("[AUDIO] Set interface Speaker to alternate setting %d\n", alt);
        spk_active = alt;
    }

    return true;
}

void tud_hid_set_report_cb(uint8_t itf, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer,
                           uint16_t bufsize) {
    (void) itf;
    (void) report_id;
    (void) report_type;
    (void) buffer;
    (void) bufsize;

    if (is_pico_cmd(report_id)) {
        printf("[HID] Receive 0xf6 setting config, funcid:0x%02X\n", buffer[0]);
        pico_cmd_set(report_id, buffer, bufsize);
        return;
    }

    if (report_id == 0) {
        switch (buffer[0]) {
            case 0x02: {
                set_state_data(buffer + 1, bufsize - 1);
                if (spk_active) {
                    break;
                }
                uint8_t outputData[78];
                outputData[0] = 0x31;
                outputData[1] = reportSeqCounter << 4;
                if (++reportSeqCounter == 256) {
                    reportSeqCounter = 0;
                }
                outputData[2] = 0x10;
                memcpy(outputData + 3, buffer + 1, bufsize - 1);
                bt_write(outputData, sizeof(outputData));
                break;
            }
        }
    }
    if (report_id == 0x80 ||
        report_id == 0x60 ||
        report_id == 0x62 ||
        report_id == 0x61) {
        set_feature_data(report_id, const_cast<uint8_t *>(buffer), bufsize);
        return;
    }
}

int main() {
    // GPIO23 ON immediately — board is alive before any SDK init.
    // This is safe: gpio_init() only touches RP2350B registers, no CYW43 needed.
    status_led_init();

    // 1.25V required for stable 360 MHz on RP2350B.
    vreg_set_voltage(VREG_VOLTAGE_1_25);
    sleep_ms(1000);
    set_sys_clock_khz(320000, true);

    board_init();
    tusb_rhport_init_t dev_init = {
        .role = TUSB_ROLE_DEVICE,
        .speed = TUSB_SPEED_FULL
    };
    tusb_init(BOARD_TUD_RHPORT, &dev_init);
#if !ENABLE_SERIAL
    tud_disconnect();
#endif
    board_init_after_tusb();
#if ENABLE_SERIAL
    stdio_usb_init();
#endif

    if (cyw43_arch_init()) {
        printf("Failed to initialize CYW43\n");
        // Leave GPIO23 ON as error indicator — system is halted
        return 1;
    }

    // CYW43 ready — turn off boot indicator, system is ready to pair
    status_led_cyw43_ready();
    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);

#if ENABLE_BATT_LED
    battery_led_init();
#endif

#if !ENABLE_SERIAL
    if (watchdog_caused_reboot()) {
        printf("Rebooted by Watchdog!\n");
        // Alternate both LEDs to distinguish watchdog reboot from clean boot.
        // GPIO23 is direct GPIO; CYW43 LED goes through driver (already inited).
        for (int i = 0; i < 6; i++) {
            const bool phase = (i % 2 == 0);
            gpio_put(23, phase);
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, !phase);
            sleep_ms(250);
        }
        // Restore both to OFF after the sequence
        gpio_put(23, false);
        cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
    } else {
        printf("Clean boot\n");
    }
#endif

    critical_section_init(&report_cs);

    config_load();

    bt_init();
    bt_register_data_callback(on_bt_data);

    audio_init();

#if !ENABLE_SERIAL
    watchdog_enable(1000, true);
#endif

    while (1) {
#if !ENABLE_SERIAL
        watchdog_update();
#endif
        cyw43_arch_poll();
        tud_task();
        audio_loop();
        interrupt_loop();
        status_led_tick();
#if ENABLE_BATT_LED
        battery_led_tick();
#endif
    }
}
