// Created by awalol on 2026/3/5.
// v2-opt — fifo depth 2->4, cached audio_gain, Waveshare RP2350B-Plus-W
// v5     — audio packets routed to priority_send_fifo via bt_write(..., true)

#include "audio.h"
#include "bt.h"
#include "resample.h"
#include "tusb.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include "opus.h"
#include "utils.h"
#include "pico/multicore.h"
#include "pico/util/queue.h"
#include "config.h"
#include "usb.h"

#define INPUT_CHANNELS  4
#define OUTPUT_CHANNELS 2
#define SAMPLE_SIZE     64
#define REPORT_SIZE     398
#define REPORT_ID       0x36

// Opus complexity is set by CMakeLists.txt OPUS_COMPLEXITY (default 4).
// At 360 MHz Core 1 handles complexity 4 without stuttering.
// Reduce the CMake cache variable to 2 if you observe audio dropouts.
#ifndef OPUS_COMPLEXITY
#define OPUS_COMPLEXITY 4
#endif

using std::clamp;
using std::max;

static WDL_Resampler resampler;
static uint8_t reportSeqCounter = 0;
static uint8_t packetCounter    = 0;
static bool    plug_headset     = false;

alignas(8) static uint32_t audio_core1_stack[8192];
queue_t audio_fifo;
static uint8_t opus_buf[200];
critical_section_t opus_cs;

struct audio_raw_element {
    float data[512 * 2];
};

uint8_t state_data[63] = {
    0xfd, 0xf7, 0x0, 0x0, 0x7f, 0x7f,
    0xff, 0x9, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
    0x0, 0xa, 0x7, 0x0, 0x0, 0x2, 0x1, 0x00, 0xff, 0xd7, 0x00,
};

void set_state_data(const uint8_t* data, const uint8_t len) {
    memcpy(state_data, data, len);
}

void set_headset(bool state) {
    plug_headset = state;
}

void audio_reset_opus_buf() {
    critical_section_enter_blocking(&opus_cs);
    memset(opus_buf, 0, sizeof(opus_buf));
    critical_section_exit(&opus_cs);
}

// Cached audio gain — powf(10, x/20) is expensive.
// Cache it and only recompute when volume changes.
// Removes a powf() call from every audio_loop() tick on Core 0.
static float cached_audio_gain     = 0.0f;
static float cached_speaker_volume = -999.0f; // sentinel: force first compute

static float get_audio_gain() {
    const float vol = get_config().speaker_volume;
    if (vol != cached_speaker_volume) {
        cached_speaker_volume = vol;
        cached_audio_gain     = powf(10.0f, vol / 20.0f);
    }
    return mute[0] ? 0.0f : cached_audio_gain;
}

void audio_loop() {
    static uint32_t silence_ticks = 0;

    if (!tud_audio_available()) {
        if (++silence_ticks == 10) {
            uint8_t pkt[REPORT_SIZE]{};
            pkt[0] = REPORT_ID;
            pkt[1] = reportSeqCounter << 4;
            reportSeqCounter = (reportSeqCounter + 1) & 0x0F;
            pkt[2] = 0x11 | 0 << 6 | 1 << 7;
            pkt[3] = 7;
            pkt[4] = 0b11111110;
            const auto buf_len = get_config().audio_buffer_length;
            pkt[5] = buf_len; pkt[6] = buf_len; pkt[7] = buf_len;
            pkt[8] = buf_len; pkt[9] = buf_len;
            pkt[10] = packetCounter++;
            pkt[11] = 0x10 | 0 << 6 | 1 << 7;
            pkt[12] = 63;
            memcpy(pkt + 13, state_data, sizeof(state_data));
            // pkt[144..343] stays zero — DS5 decodes as silence
            bt_write(pkt, sizeof(pkt), true);
        }
        return;
    }
    silence_ticks = 0;

    int16_t  raw[192];
    uint32_t bytes_read = tud_audio_read(raw, sizeof(raw));
    int      frames     = bytes_read / (INPUT_CHANNELS * sizeof(int16_t));
    if (frames == 0) return;

    static float audio_buf[512 * 2];
    static uint  audio_buf_pos = 0;

    WDL_ResampleSample *in_buf;
    int nframes = resampler.ResamplePrepare(frames, OUTPUT_CHANNELS, &in_buf);

    const float audio_gain   = get_audio_gain();
    const float haptics_gain = get_config().haptics_gain;

    for (int i = 0; i < nframes; i++) {
        audio_buf[audio_buf_pos++] = raw[i * INPUT_CHANNELS]     / 32768.0f * audio_gain;
        audio_buf[audio_buf_pos++] = raw[i * INPUT_CHANNELS + 1] / 32768.0f * audio_gain;

        if (audio_buf_pos == 512 * 2) {
            static audio_raw_element element{};
            memcpy(element.data, audio_buf, 512 * 2 * 4);
            // Drop oldest frame if full rather than silently losing newest data
            if (queue_is_full(&audio_fifo)) {
                queue_try_remove(&audio_fifo, NULL);
            }
            if (!queue_try_add(&audio_fifo, &element)) {
                printf("[Audio] Warning: audio_fifo add failed\n");
            }
            audio_buf_pos = 0;
        }

        in_buf[i * 2]     = static_cast<WDL_ResampleSample>(clamp(
            raw[i * INPUT_CHANNELS + 2] / 32768.0f * haptics_gain, -1.0f, 1.0f));
        in_buf[i * 2 + 1] = static_cast<WDL_ResampleSample>(clamp(
            raw[i * INPUT_CHANNELS + 3] / 32768.0f * haptics_gain, -1.0f, 1.0f));
    }

    static WDL_ResampleSample out_buf[SAMPLE_SIZE];
    const int out_frames = resampler.ResampleOut(out_buf, nframes, nframes / 4, OUTPUT_CHANNELS);

    static int8_t haptic_buf[SAMPLE_SIZE];
    static int    haptic_buf_pos = 0;

    for (int i = 0; i < out_frames; i++) {
        int val_l = static_cast<int>(out_buf[i * 2]     * 127.0f);
        int val_r = static_cast<int>(out_buf[i * 2 + 1] * 127.0f);
        haptic_buf[haptic_buf_pos++] = (int8_t) clamp(val_l, -128, 127);
        haptic_buf[haptic_buf_pos++] = (int8_t) clamp(val_r, -128, 127);

        if (haptic_buf_pos != SAMPLE_SIZE) continue;

        uint8_t pkt[REPORT_SIZE]{};
        pkt[0] = REPORT_ID;
        pkt[1] = reportSeqCounter << 4;
        reportSeqCounter = (reportSeqCounter + 1) & 0x0F;
        pkt[2] = 0x11 | 0 << 6 | 1 << 7;
        pkt[3] = 7;
        pkt[4] = 0b11111110;

        const auto buf_len = get_config().audio_buffer_length;
        pkt[5] = buf_len;
        pkt[6] = buf_len;
        pkt[7] = buf_len;
        pkt[8] = buf_len;
        pkt[9] = buf_len;

        pkt[10] = packetCounter++;
        pkt[11] = 0x10 | 0 << 6 | 1 << 7;
        pkt[12] = 63;
        memcpy(pkt + 13, state_data, sizeof(state_data));
        pkt[76] = 0x12 | 0 << 6 | 1 << 7;
        pkt[77] = SAMPLE_SIZE;
        memcpy(pkt + 78, haptic_buf, SAMPLE_SIZE);
        pkt[142] = (plug_headset ? 0x16 : 0x13) | 0 << 6 | 1 << 7;
        pkt[143] = 200;

        critical_section_enter_blocking(&opus_cs);
        memcpy(pkt + 144, opus_buf, 200);
        critical_section_exit(&opus_cs);

        // Priority flag = true — audio packets go to priority_send_fifo,
        // ahead of HID output reports in send_fifo. This is the stutter fix.
        bt_write(pkt, sizeof(pkt), true);
        haptic_buf_pos = 0;
    }
}

void audio_init() {
    resampler.SetMode(true, 0, false);
    resampler.SetRates(48000, 3000);
    resampler.SetFeedMode(true);
    resampler.Prealloc(2, 24, 6);

    // Depth 2->4 — gives Core 1 (Opus encoder) more headroom before
    // frames are dropped. At 360 MHz Core 1 encodes ~10ms frames; depth 4
    // covers ~40ms of burst without dropping.
    queue_init(&audio_fifo, sizeof(audio_raw_element), 4);

    critical_section_init(&opus_cs);
    multicore_launch_core1_with_stack(core1_entry, audio_core1_stack, sizeof(audio_core1_stack));
}

static OpusEncoder  *encoder = nullptr;
static WDL_Resampler resampler_audio;

void core1_entry() {
    int error = 0;
    encoder = opus_encoder_create(48000, 2, OPUS_APPLICATION_AUDIO, &error);
    if (error != 0) {
        printf("[Audio] OpusEncoder create failed\n");
        return;
    }

    opus_encoder_ctl(encoder, OPUS_SET_EXPERT_FRAME_DURATION(OPUS_FRAMESIZE_10_MS));
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(200 * 8 * 100));
    opus_encoder_ctl(encoder, OPUS_SET_VBR(false));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(OPUS_COMPLEXITY));

    resampler_audio.SetMode(true, 0, false);
    resampler_audio.SetRates(51200, 48000);
    resampler_audio.SetFeedMode(true);
    resampler_audio.Prealloc(2, 512, 480);

    while (true) {
        static audio_raw_element audio_element{};
        queue_remove_blocking(&audio_fifo, &audio_element);

        // Resample 512 -> 480 frames to fix noise. Credit: @Junhoo
        WDL_ResampleSample *in_buf;
        int nframes = resampler_audio.ResamplePrepare(512, 2, &in_buf);
        for (int i = 0; i < nframes * 2; i++) {
            in_buf[i] = audio_element.data[i];
        }

        static WDL_ResampleSample out_buf[480 * 2];
        resampler_audio.ResampleOut(out_buf, nframes, 480, 2);

        static uint8_t out[200];
        (void) opus_encode_float(encoder, out_buf, 480, out, 200);

        critical_section_enter_blocking(&opus_cs);
        memcpy(opus_buf, out, 200);
        critical_section_exit(&opus_cs);
    }
}
