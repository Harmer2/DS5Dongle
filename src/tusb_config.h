#ifndef _TUSB_CONFIG_H
#define _TUSB_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// --- MCU ---
#define CFG_TUSB_MCU OPT_MCU_RP2040
#define CFG_TUSB_OS OPT_OS_PICO
#define CFG_TUSB_RHPORT0_MODE OPT_MODE_DEVICE

// --- Memory ---
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN __attribute__((aligned(4)))

// --- Device ---
#define CFG_TUD_ENDPOINT0_SIZE 64

// --- Audio Configuration (MUST BE BEFORE CFG_TUD_AUDIO) ---
#define CFG_TUD_AUDIO_FUNC_1_DESC_LEN 109
#define CFG_TUD_AUDIO_FUNC_1_N_AS_INT 2
#define CFG_TUD_AUDIO_FUNC_1_CTRL_BUF_SZ 64
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_RX 4
#define CFG_TUD_AUDIO_FUNC_1_N_CHANNELS_TX 2
#define CFG_TUD_AUDIO_FUNC_1_SAMPLE_RATE 48000
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_RX 2
#define CFG_TUD_AUDIO_FUNC_1_N_BYTES_PER_SAMPLE_TX 2
#define CFG_TUD_AUDIO_EP_SZ_IN 196
#define CFG_TUD_AUDIO_EP_SZ_OUT 392
#define CFG_TUD_AUDIO_FUNC_1_EP_OUT_SW_BUF_SZ (392 * 6)
#define CFG_TUD_AUDIO_FUNC_1_EP_IN_SW_BUF_SZ (196 * 4)
#define CFG_TUD_AUDIO_ENABLE_FEEDBACK_EP 1
#define CFG_TUD_AUDIO_ENABLE_EP_OUT 1
#define CFG_TUD_AUDIO_ENABLE_EP_IN 1

// --- Class enable ---
#define CFG_TUD_HID 1
#define CFG_TUD_AUDIO 1
#define CFG_TUD_MSC 0
#define CFG_TUD_MIDI 0
#define CFG_TUD_VENDOR 0

// CDC: controlled by ENABLE_SERIAL (0 or 1 from CMake)
#if ENABLE_SERIAL
#define CFG_TUD_CDC 1
#else
#define CFG_TUD_CDC 0
#endif

// --- HID ---
#define CFG_TUD_HID_EP_BUFSIZE 64

#ifdef __cplusplus
}
#endif

#endif // _TUSB_CONFIG_H
