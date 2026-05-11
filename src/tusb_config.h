#ifndef _TUSB_CONFIG_H_
#define _TUSB_CONFIG_H_

#ifdef __cplusplus
extern "C" {
#endif

// Board-specific
#include "pico/types.h"

// MCU
#define CFG_TUSB_MCU                OPT_MCU_RP2040
#define CFG_TUSB_OS                 OPT_OS_PICO
#define CFG_TUSB_RHPORT0_MODE       OPT_MODE_DEVICE

// Memory
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN          __attribute__((aligned(4)))

// Device config
#define CFG_TUD_ENDPOINT0_SIZE      64

// Class drivers
#define CFG_TUD_HID                 1
#define CFG_TUD_AUDIO               1
#define CFG_TUD_CDC                 ENABLE_SERIAL
#define CFG_TUD_MSC                 0
#define CFG_TUD_MIDI                0
#define CFG_TUD_VENDOR              0

// HID buffer size
#define CFG_TUD_HID_EP_BUFSIZE      64

// Audio Class Driver Configuration
#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN                       109
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT                       2
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ                    64
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX                  4
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX                  2
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE                    48000
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX          2
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX          2

// Audio Endpoint Sizes - THE CRITICAL DEFINES
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SZ_MAX                   196
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SZ_MAX                  392

// Audio Software Buffers
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ               (392 * 6)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ                (196 * 4)

// Audio Feedback
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP                    1
#define CFG_TUD_AUDIO_ENABLE_EP_OUT                         1
#define CFG_TUD_AUDIO_ENABLE_EP_IN                          1

#ifdef __cplusplus
}
#endif

#endif /* _TUSB_CONFIG_H_ */
