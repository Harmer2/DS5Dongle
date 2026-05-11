#include "bt.h"
#include "usb.h"
#include "utils.h"
#include "config.h"

#include <string.h>
#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"

#include "btstack.h"
#include "btstack_run_loop.h"
#include "classic/sdp_util.h"

// === Constants ===
#define L2CAP_PSM_HID_CONTROL   0x0011
#define L2CAP_PSM_HID_INTERRUPT 0x0013
#define L2CAP_MTU               672

#define DS_COD_MAJOR            0x0500
#define DS_COD_MINOR_GAMEPAD    0x0008
#define DS_COD                  0x002508

#define HID_HEADER_DATA_INPUT   0xA1
#define HID_HEADER_DATA_OUTPUT  0xA2
#define HID_HEADER_CONTROL      0xA3

#define SEND_FIFO_SIZE  4

// === State ===
typedef enum {
    BT_STATE_IDLE,
    BT_STATE_SCANNING,
    BT_STATE_CONNECTING,
    BT_STATE_CONTROL_OPEN,
    BT_STATE_CONNECTED,
    BT_STATE_DISCONNECTING,
} bt_state_t;

static bt_state_t state = BT_STATE_IDLE;
static bd_addr_t ds_addr;
static bool ds_addr_valid = false;

static uint16_t l2cap_control_cid = 0;
static uint16_t l2cap_interrupt_cid = 0;
static hci_con_handle_t acl_handle = HCI_CON_HANDLE_INVALID;

static absolute_time_t led_next_toggle;
static bool led_state = false;

static absolute_time_t last_activity;

typedef struct {
    uint8_t data[400];
    uint16_t len;
} send_entry_t;

static send_entry_t send_fifo[SEND_FIFO_SIZE];
static uint8_t fifo_head = 0;
static uint8_t fifo_tail = 0;
static uint8_t fifo_count = 0;

static uint8_t feature_report_buf[256];
static uint16_t feature_report_len = 0;
static bool feature_report_pending = false;

// === Forward declarations ===
static void bt_start_inquiry(void);
static void bt_packet_handler(uint8_t packet_type, uint16_t channel,
                               uint8_t* packet, uint16_t size);
static void bt_send_next(void);

// === Initialization ===
void bt_init(void) {
    l2cap_init();

    static btstack_packet_callback_registration_t hci_event_callback;
    hci_event_callback.callback = &bt_packet_handler;
    hci_add_event_handler(&hci_event_callback);

    l2cap_register_service(&bt_packet_handler, L2CAP_PSM_HID_CONTROL, L2CAP_MTU, gap_get_security_level());
    l2cap_register_service(&bt_packet_handler, L2CAP_PSM_HID_INTERRUPT, L2CAP_MTU, gap_get_security_level());

    gap_ssp_set_io_capability(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
    gap_set_security_level(LEVEL_2);

    gap_set_local_name("DS5 Bridge");
    gap_set_class_of_device(0x002540);

    gap_discoverable_control(1);
    gap_connectable_control(1);

    hci_power_control(HCI_POWER_ON);

    last_activity = get_absolute_time();
    led_next_toggle = get_absolute_time();
}

// === Public API ===
bool bt_is_connected(void) {
    return (state == BT_STATE_CONNECTED);
}

void bt_write(const uint8_t* data, uint16_t len) {
    if (state != BT_STATE_CONNECTED) return;
    if (len > 400) return;

    if (fifo_count >= SEND_FIFO_SIZE) {
        fifo_tail = (fifo_tail + 1) % SEND_FIFO_SIZE;
        fifo_count--;
    }

    memcpy(send_fifo[fifo_head].data, data, len);
    send_fifo[fifo_head].len = len;
    fifo_head = (fifo_head + 1) % SEND_FIFO_SIZE;
    fifo_count++;

    bt_send_next();
}

void bt_send_feature_report(const uint8_t* data, uint16_t len) {
    if (state != BT_STATE_CONNECTED) return;
    if (l2cap_control_cid == 0) return;

    uint8_t buf[256 + 2];
    buf[0] = 0x53; // SET_REPORT | Feature
    buf[1] = data[0];
    memcpy(&buf[2], data + 1, len - 1);
    l2cap_send(l2cap_control_cid, buf, len + 1);
}

const uint8_t* bt_get_feature_report(uint16_t* out_len) {
    if (feature_report_pending) {
        feature_report_pending = false;
        *out_len = feature_report_len;
        return feature_report_buf;
    }
    *out_len = 0;
    return NULL;
}

// === Poll ===
void bt_poll(void) {
    if (state == BT_STATE_SCANNING || state == BT_STATE_CONNECTING) {
        if (absolute_time_diff_us(led_next_toggle, get_absolute_time()) > 0) {
            led_state = !led_state;
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, led_state);
            led_next_toggle = make_timeout_time_ms(250);
        }
    }

    bt_send_next();

    const auto& cfg = get_config();
    if (!cfg.disable_inactive_disconnect && state == BT_STATE_CONNECTED) {
        int64_t inactive_us = absolute_time_diff_us(last_activity, get_absolute_time());
        if (inactive_us > (int64_t)cfg.inactive_time * 60LL * 1000000LL) {
            if (acl_handle != HCI_CON_HANDLE_INVALID) {
                gap_disconnect(acl_handle);
            }
            state = BT_STATE_DISCONNECTING;
        }
    }
}

// === Send next queued report ===
static void bt_send_next(void) {
    if (fifo_count == 0) return;
    if (l2cap_interrupt_cid == 0) return;
    if (!l2cap_can_send_packet_now(l2cap_interrupt_cid)) return;

    send_entry_t* entry = &send_fifo[fifo_tail];

    uint8_t buf[402];
    buf[0] = HID_HEADER_DATA_OUTPUT;
    memcpy(&buf[1], entry->data, entry->len);
    l2cap_send(l2cap_interrupt_cid, buf, entry->len + 1);

    fifo_tail = (fifo_tail + 1) % SEND_FIFO_SIZE;
    fifo_count--;
}

// === Start inquiry ===
static void bt_start_inquiry(void) {
    state = BT_STATE_SCANNING;
    gap_inquiry_start(5);
}

// === Packet handler ===
static void bt_packet_handler(uint8_t packet_type, uint16_t channel,
                               uint8_t* packet, uint16_t size) {
    (void)channel;

    if (packet_type == HCI_EVENT_PACKET) {
        uint8_t event_type = hci_event_packet_get_type(packet);

        switch (event_type) {
            case BTSTACK_EVENT_STATE: {
                if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
                    bt_start_inquiry();
                }
                break;
            }

            case GAP_EVENT_INQUIRY_RESULT: {
                uint32_t cod = gap_event_inquiry_result_get_class_of_device(packet);
                if ((cod & 0x00FFFF) == DS_COD || (cod & 0x001F00) == DS_COD_MAJOR) {
                    gap_event_inquiry_result_get_bd_addr(packet, ds_addr);
                    ds_addr_valid = true;
#if ENABLE_SERIAL
                    printf("[BT] Found DualSense: %s CoD=0x%06X\n",
                           bd_addr_to_str(ds_addr), (unsigned)cod);
#endif
                    gap_inquiry_stop();
                }
                break;
            }

            case GAP_EVENT_INQUIRY_COMPLETE: {
                if (ds_addr_valid && state == BT_STATE_SCANNING) {
                    state = BT_STATE_CONNECTING;
                    l2cap_create_channel(&bt_packet_handler, ds_addr,
                                         L2CAP_PSM_HID_CONTROL, L2CAP_MTU, &l2cap_control_cid);
                } else if (state == BT_STATE_SCANNING) {
                    bt_start_inquiry();
                }
                break;
            }

            case L2CAP_EVENT_CHANNEL_OPENED: {
                uint8_t status = l2cap_event_channel_opened_get_status(packet);
                uint16_t psm = l2cap_event_channel_opened_get_psm(packet);
                uint16_t cid = l2cap_event_channel_opened_get_local_cid(packet);

                if (status != 0) {
#if ENABLE_SERIAL
                    printf("[BT] L2CAP open failed: PSM=0x%04X status=%d\n", psm, status);
#endif
                    state = BT_STATE_IDLE;
                    bt_start_inquiry();
                    break;
                }

                // Store ACL handle for disconnect
                acl_handle = l2cap_event_channel_opened_get_handle(packet);

                if (psm == L2CAP_PSM_HID_CONTROL) {
                    l2cap_control_cid = cid;
                    state = BT_STATE_CONTROL_OPEN;
#if ENABLE_SERIAL
                    printf("[BT] Control channel open (CID=%d)\n", cid);
#endif
                    l2cap_create_channel(&bt_packet_handler, ds_addr,
                                         L2CAP_PSM_HID_INTERRUPT, L2CAP_MTU, &l2cap_interrupt_cid);
                } else if (psm == L2CAP_PSM_HID_INTERRUPT) {
                    l2cap_interrupt_cid = cid;
                    state = BT_STATE_CONNECTED;
                    last_activity = get_absolute_time();
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                    led_state = true;
#if ENABLE_SERIAL
                    printf("[BT] Interrupt channel open (CID=%d) - CONNECTED\n", cid);
#endif
                }
                break;
            }

            case L2CAP_EVENT_CHANNEL_CLOSED: {
                uint16_t cid = l2cap_event_channel_closed_get_local_cid(packet);
                if (cid == l2cap_interrupt_cid) l2cap_interrupt_cid = 0;
                if (cid == l2cap_control_cid) l2cap_control_cid = 0;
                if (l2cap_interrupt_cid == 0 && l2cap_control_cid == 0) {
                    state = BT_STATE_IDLE;
                    acl_handle = HCI_CON_HANDLE_INVALID;
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
                    led_state = false;
#if ENABLE_SERIAL
                    printf("[BT] Disconnected - restarting scan\n");
#endif
                    ds_addr_valid = false;
                    bt_start_inquiry();
                }
                break;
            }

            case L2CAP_EVENT_INCOMING_CONNECTION: {
                uint16_t cid = l2cap_event_incoming_connection_get_local_cid(packet);
                l2cap_accept_connection(cid);
#if ENABLE_SERIAL
                uint16_t psm = l2cap_event_incoming_connection_get_psm(packet);
                printf("[BT] Accepting incoming L2CAP PSM=0x%04X CID=%d\n", psm, cid);
#endif
                break;
            }

            case HCI_EVENT_USER_CONFIRMATION_REQUEST:
                hci_event_user_confirmation_request_get_bd_addr(packet, ds_addr);
                gap_ssp_confirmation_response(ds_addr);
                break;

            case HCI_EVENT_PIN_CODE_REQUEST:
                gap_pin_code_response(ds_addr, "0000");
                break;

            case HCI_EVENT_LINK_KEY_REQUEST:
                // TODO: Implement persistent link key storage
                hci_send_cmd(&hci_link_key_request_negative_reply, ds_addr);
                break;

            default:
                break;
        }
    }
    else if (packet_type == L2CAP_DATA_PACKET) {
        if (size < 2) return;

        last_activity = get_absolute_time();

        uint8_t hid_header = packet[0];
        uint8_t report_id = packet[1];

        if (hid_header == HID_HEADER_DATA_INPUT && report_id == 0x31) {
            if (size >= 12) {
                usb_send_hid_report(packet + 1, size - 1);
            }
        }
        else if (hid_header == HID_HEADER_CONTROL) {
            uint16_t payload_len = size - 1;
            if (payload_len > sizeof(feature_report_buf)) {
                payload_len = sizeof(feature_report_buf);
            }
            memcpy(feature_report_buf, packet + 1, payload_len);
            feature_report_len = payload_len;
            feature_report_pending = true;
        }
    }
}
