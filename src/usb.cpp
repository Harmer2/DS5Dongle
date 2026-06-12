//
// Created by awalol on 2026/3/4.
//

#include "tusb.h"
#include "bsp/board_api.h"
#include "config.h"

#ifndef ENABLE_AUDIO
#define ENABLE_AUDIO 1
#endif

#if ENABLE_AUDIO

uint8_t mute[2]; // 0: SPEAKER(0x02) 1: MIC(0x05)
float volume[2] = {-100.0f, 0.0f}; // 0: SPEAKER(0x02) 1: MIC(0x05)

#define UAC1_ENTITY_SPK_FEATURE_UNIT    0x02
#define UAC1_ENTITY_MIC_FEATURE_UNIT    0x05

static bool audio10_set_req_entity(tusb_control_request_t const *p_request, uint8_t *pBuff) {
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);
    uint8_t index = entityID == UAC1_ENTITY_SPK_FEATURE_UNIT ? 0 : 1;

    if (entityID == UAC1_ENTITY_SPK_FEATURE_UNIT || entityID == UAC1_ENTITY_MIC_FEATURE_UNIT) {
        switch (ctrlSel) {
            case AUDIO10_FU_CTRL_MUTE:
                switch (p_request->bRequest) {
                    case AUDIO10_CS_REQ_SET_CUR:
                        TU_VERIFY(p_request->wLength == 1);
                        mute[index] = pBuff[0];
                        TU_LOG2("    Set Mute: %d of entity: %u\r\n", mute[index], entityID);
                        return true;
                    default:
                        return false;
                }

            case AUDIO10_FU_CTRL_VOLUME:
                switch (p_request->bRequest) {
                    case AUDIO10_CS_REQ_SET_CUR:
                        TU_VERIFY(p_request->wLength == 2);
                        volume[index] = static_cast<float>(*reinterpret_cast<int16_t const *>(pBuff)) / 256;
                        if (entityID == UAC1_ENTITY_SPK_FEATURE_UNIT) {
                            auto config = get_config();
                            config.speaker_volume = volume[index];
                            set_config(config);
                        }
                        TU_LOG2("    Set Volume: %d dB of entity: %u\r\n", volume[index], entityID);
                        return true;
                    default:
                        return false;
                }

            default:
                TU_BREAKPOINT();
                return false;
        }
    }

    return false;
}

static bool audio10_get_req_entity(uint8_t rhport, tusb_control_request_t const *p_request) {
    uint8_t channelNum = TU_U16_LOW(p_request->wValue);
    uint8_t ctrlSel = TU_U16_HIGH(p_request->wValue);
    uint8_t entityID = TU_U16_HIGH(p_request->wIndex);
    uint8_t index = entityID == UAC1_ENTITY_SPK_FEATURE_UNIT ? 0 : 1;

    if (entityID == UAC1_ENTITY_SPK_FEATURE_UNIT || entityID == UAC1_ENTITY_MIC_FEATURE_UNIT) {
        switch (ctrlSel) {
            case AUDIO10_FU_CTRL_MUTE:
                TU_LOG2("    Get Mute of entity: %u\r\n", entityID);
                return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &mute[index], 1);

            case AUDIO10_FU_CTRL_VOLUME:
                switch (p_request->bRequest) {
                    case AUDIO10_CS_REQ_GET_CUR:
                        TU_LOG2("    Get Volume of entity: %u\r\n", entityID); {
                            if (entityID == UAC1_ENTITY_SPK_FEATURE_UNIT) {
                                volume[index] = get_config().speaker_volume;
                            }
                            int16_t vol = volume[index] * 256;
                            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &vol, sizeof(vol));
                        }

                    case AUDIO10_CS_REQ_GET_MIN:
                        TU_LOG2("    Get Volume min of entity: %u\r\n", entityID); {
                            uint8_t min[2];
                            if (entityID == UAC1_ENTITY_SPK_FEATURE_UNIT) {
                                min[0] = 0x00; min[1] = 0x9c;
                            } else {
                                min[0] = 0x00; min[1] = 0x00;
                            }
                            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &min, sizeof(min));
                        }

                    case AUDIO10_CS_REQ_GET_MAX:
                        TU_LOG2("    Get Volume max of entity: %u\r\n", entityID); {
                            uint8_t max[2];
                            if (entityID == UAC1_ENTITY_SPK_FEATURE_UNIT) {
                                max[0] = 0x00; max[1] = 0x00;
                            } else {
                                max[0] = 0x00; max[1] = 0x30;
                            }
                            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &max, sizeof(max));
                        }

                    case AUDIO10_CS_REQ_GET_RES:
                        TU_LOG2("    Get Volume res of entity: %u\r\n", entityID); {
                            uint8_t res[2];
                            if (entityID == UAC1_ENTITY_SPK_FEATURE_UNIT) {
                                res[0] = 0x00; res[1] = 0x01;
                            } else {
                                res[0] = 0x7a; res[1] = 0x00;
                            }
                            return tud_audio_buffer_and_schedule_control_xfer(rhport, p_request, &res, sizeof(res));
                        }

                    default:
                        TU_BREAKPOINT();
                        return false;
                }
                break;

            default:
                TU_BREAKPOINT();
                return false;
        }
    }

    return false;
}

bool tud_audio_get_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request) {
    (void) rhport;
    return audio10_get_req_entity(rhport, p_request);
}

bool tud_audio_set_req_entity_cb(uint8_t rhport, tusb_control_request_t const *p_request, uint8_t *buf) {
    (void) rhport;
    return audio10_set_req_entity(p_request, buf);
}

#endif // ENABLE_AUDIO

void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len) {
    (void) instance;
    (void) len;
}
