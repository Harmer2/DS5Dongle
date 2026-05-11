#include "audio.h"
#include "config.h"
#include "bt.h"
#include "utils.h"

#include <string.h>
#include <math.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/critical_section.h"
#include "hardware/sync.h"
#include "tusb.h"

#include "opus.h"
#include "WDL/resample.h"

// === Constants ===
#define SAMPLE_RATE_IN      48000
#define SAMPLE_RATE_HAPTIC  3000
#define OUTPUT_CHANNELS     2
#define OPUS_FRAME_SAMPLES  480   // 10ms at 48kHz
#define OPUS_BITRATE        160000
#define OPUS_MAX_PACKET     200
#define BT_REPORT_SIZE      398
#define HAPTIC_RATIO        16    // 48000 / 3000

#define AUDIO_FIFO_DEPTH    6

#define CORE1_STACK_SIZE    (8192)
static uint32_t core1_stack[CORE1_STACK_SIZE];

// === Audio element passed Core0 → Core1 via queue ===
typedef struct {
    float samples[OPUS_FRAME_SAMPLES * OUTPUT_CHANNELS]; // 960 floats = 3840 bytes
} audio_raw_element;

// === State ===
static queue_t audio_fifo;
static critical_section_t opus_cs;

static uint8_t opus_buf[OPUS_MAX_PACKET];
static int opus_buf_len = 0;

static int16_t haptic_buf[OPUS_FRAME_SAMPLES / HAPTIC_RATIO * OUTPUT_CHANNELS]; // 60 samples
static int haptic_buf_len = 0;

static WDL_Resampler haptic_resampler;

static int16_t usb_audio_buf[4 * 48];

static float speaker_accum[OPUS_FRAME_SAMPLES * OUTPUT_CHANNELS];
static float haptic_accum[OPUS_FRAME_SAMPLES * OUTPUT_CHANNELS];
static int accum_pos = 0;

static float cached_speaker_gain = 1.0f;
static float cached_haptics_gain = 1.0f;

// === DualSense BT output report state (LED, rumble, triggers) ===
// Report 0x31 = BT output. First byte after HID header (0xA2) is 0x31.
// Total payload: 78 bytes (report data) + CRC = varies by protocol revision.
// We store the raw host output data and rebuild into BT format on send.
#define DS_BT_OUTPUT_REPORT_SIZE 78

static uint8_t ds_output_state[DS_BT_OUTPUT_REPORT_SIZE];
static bool ds_output_dirty = false;
static critical_section_t output_cs;

static bool headset_connected = false;

// === Core 1 — Opus Encoding ===
static void __time_critical_func(core1_opus_task)(void) {
    int err;
    OpusEncoder* encoder = opus_encoder_create(SAMPLE_RATE_IN, OUTPUT_CHANNELS,
                                                OPUS_APPLICATION_AUDIO, &err);
    if (err != OPUS_OK || !encoder) {
        while (1) tight_loop_contents();
    }

    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(OPUS_BITRATE));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(OPUS_COMPLEXITY));
    opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
    opus_encoder_ctl(encoder, OPUS_SET_LSB_DEPTH(16));

    audio_raw_element elem;
    uint8_t encode_out[OPUS_MAX_PACKET];

    while (1) {
        queue_remove_blocking(&audio_fifo, &elem);

        int encoded = opus_encode_float(encoder, elem.samples, OPUS_FRAME_SAMPLES,
                                         encode_out, OPUS_MAX_PACKET);

        if (encoded > 0 && encoded <= OPUS_MAX_PACKET) {
            critical_section_enter_blocking(&opus_cs);
            memcpy(opus_buf, encode_out, encoded);
            opus_buf_len = encoded;
            critical_section_exit(&opus_cs);
        }
    }
}

// === Initialization ===
void audio_init(void) {
    queue_init(&audio_fifo, sizeof(audio_raw_element), AUDIO_FIFO_DEPTH);
    critical_section_init(&opus_cs);
    critical_section_init(&output_cs);

    haptic_resampler.SetMode(true, 1, true);
    haptic_resampler.SetRates(SAMPLE_RATE_IN, SAMPLE_RATE_HAPTIC);

    const config_t& cfg = get_config();
    cached_speaker_gain = powf(10.0f, cfg.speaker_volume / 20.0f);
    cached_haptics_gain = cfg.haptics_gain;

    accum_pos = 0;
    memset(speaker_accum, 0, sizeof(speaker_accum));
    memset(haptic_accum, 0, sizeof(haptic_accum));
    memset(ds_output_state, 0, sizeof(ds_output_state));

    multicore_launch_core1_with_stack(core1_opus_task, core1_stack, sizeof(core1_stack));
}

void audio_update_gains(void) {
    const config_t& cfg = get_config();
    cached_speaker_gain = powf(10.0f, cfg.speaker_volume / 20.0f);
    cached_haptics_gain = cfg.haptics_gain;
}

// === set_state_data: Host sends USB output report 0x02, we translate to BT 0x31 ===
void set_state_data(const uint8_t *data, uint16_t len) {
    if (!bt_is_connected()) return;

    // DualSense USB output report 0x02 is 48 bytes (excluding report ID).
    // DualSense BT output report 0x31 is 78 bytes payload.
    // The mapping: USB byte offsets shift in BT report.
    // BT report structure:
    //   [0]    = 0x31 (report ID)
    //   [1]    = sequence tag (auto-increment)
    //   [2]    = USB byte 0 (valid flags 0)
    //   [3]    = USB byte 1 (valid flags 1)
    //   [4..] = USB byte 2+ (motor, LED, trigger data)
    //   [74..77] = CRC32

    static uint8_t seq = 0;

    uint8_t report[DS_BT_OUTPUT_REPORT_SIZE];
    memset(report, 0, DS_BT_OUTPUT_REPORT_SIZE);

    report[0] = 0x31;       // BT output report ID
    report[1] = seq++;      // Sequence number

    // Copy USB output data into BT report offset +2
    // USB report 0x02 data starts after report ID (which the host already stripped)
    uint16_t copy_len = len;
    if (copy_len > DS_BT_OUTPUT_REPORT_SIZE - 6) {  // Leave room for header + CRC
        copy_len = DS_BT_OUTPUT_REPORT_SIZE - 6;
    }
    memcpy(&report[2], data, copy_len);

    // Set headset connected flag if applicable
    // Byte 2 = valid flags 0: bit 4 = audio control enable
    // Byte 39 (USB offset 37) = plugged status flags
    if (headset_connected) {
        report[2] |= 0x10;  // Enable audio control
    }

    // CRC32 over the entire report (seed 0xEADA2D49)
    fill_output_report_checksum(report, DS_BT_OUTPUT_REPORT_SIZE);

    // Cache for reference
    critical_section_enter_blocking(&output_cs);
    memcpy(ds_output_state, report, DS_BT_OUTPUT_REPORT_SIZE);
    ds_output_dirty = false;
    critical_section_exit(&output_cs);

    // Send via BT
    bt_write(report, DS_BT_OUTPUT_REPORT_SIZE);
}

// === set_headset: Toggle headset presence in output reports ===
void set_headset(bool connected) {
    headset_connected = connected;
}

// === Pack haptics + Opus into BT audio report ===
static void pack_bt_audio_report(void) {
    // DualSense BT audio report is 0x36, 398 bytes total
    uint8_t report[BT_REPORT_SIZE];
    memset(report, 0, BT_REPORT_SIZE);

    report[0] = 0x36; // BT audio output report ID

    // Haptic data: 30 frames × 2ch × 2 bytes = 120 bytes at offset 4
    const int haptic_offset = 4;
    int haptic_samples = haptic_buf_len;
    if (haptic_samples > (int)sizeof(haptic_buf) / 2) {
        haptic_samples = sizeof(haptic_buf) / 2;
    }
    memcpy(&report[haptic_offset], haptic_buf, haptic_samples * 2);

    // Opus payload after haptics
    const int opus_offset = haptic_offset + 120;
    critical_section_enter_blocking(&opus_cs);
    int olen = opus_buf_len;
    if (olen > 0) {
        memcpy(&report[opus_offset], opus_buf, olen);
    }
    critical_section_exit(&opus_cs);

    report[2] = (uint8_t)olen;
    report[3] = (uint8_t)haptic_samples;

    fill_output_report_checksum(report, BT_REPORT_SIZE);

    bt_write(report, BT_REPORT_SIZE);
}

// === Main audio loop (Core 0) ===
void audio_loop(void) {
    if (!tud_audio_mounted()) return;
    if (!bt_is_connected()) return;

    int bytes_read = tud_audio_read(usb_audio_buf, sizeof(usb_audio_buf));
    if (bytes_read <= 0) return;

    int samples_per_channel = bytes_read / (4 * sizeof(int16_t));

    for (int i = 0; i < samples_per_channel; i++) {
        int idx = accum_pos * OUTPUT_CHANNELS;

        float l = (float)usb_audio_buf[i * 4 + 0] / 32768.0f * cached_speaker_gain;
        float r = (float)usb_audio_buf[i * 4 + 1] / 32768.0f * cached_speaker_gain;
        speaker_accum[idx + 0] = l;
        speaker_accum[idx + 1] = r;

        float hl = (float)usb_audio_buf[i * 4 + 2] / 32768.0f * cached_haptics_gain;
        float hr = (float)usb_audio_buf[i * 4 + 3] / 32768.0f * cached_haptics_gain;
        haptic_accum[idx + 0] = hl;
        haptic_accum[idx + 1] = hr;

        accum_pos++;

        if (accum_pos >= OPUS_FRAME_SAMPLES) {
            accum_pos = 0;

            // Speaker → Core 1 for Opus encoding
            audio_raw_element elem;
            memcpy(elem.samples, speaker_accum, sizeof(elem.samples));

            if (queue_is_full(&audio_fifo)) {
                audio_raw_element discard;
                queue_try_remove(&audio_fifo, &discard);
            }
            queue_try_add(&audio_fifo, &elem);

            // Haptics → Resample 48kHz to 3kHz
            WDL_ResampleSample* rsin = NULL;
            int nframes = haptic_resampler.ResamplePrepare(OPUS_FRAME_SAMPLES, OUTPUT_CHANNELS, &rsin);

            int copy_frames = (nframes < OPUS_FRAME_SAMPLES) ? nframes : OPUS_FRAME_SAMPLES;
            for (int j = 0; j < copy_frames * OUTPUT_CHANNELS; j++) {
                rsin[j] = haptic_accum[j];
            }

            float haptic_out[64 * OUTPUT_CHANNELS];
            int out_frames = haptic_resampler.ResampleOut(haptic_out, nframes,
                                                          nframes / HAPTIC_RATIO, OUTPUT_CHANNELS);

            haptic_buf_len = 0;
            for (int j = 0; j < out_frames * OUTPUT_CHANNELS && j < (int)(sizeof(haptic_buf)/sizeof(int16_t)); j++) {
                float clamped = haptic_out[j];
                if (clamped > 1.0f) clamped = 1.0f;
                if (clamped < -1.0f) clamped = -1.0f;
                haptic_buf[j] = (int16_t)(clamped * 32767.0f);
                haptic_buf_len++;
            }

            pack_bt_audio_report();
        }
    }
}
