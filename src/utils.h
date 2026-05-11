//
// Created by awalol on 2026/3/4.
// Cleaned up — removed <iostream> (pulls in ~60KB of code on embedded).
// All print_hex uses printf instead.
//

#ifndef DS5_BRIDGE_UTILS_H
#define DS5_BRIDGE_UTILS_H

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>

#include "hci_cmd.h"

inline const char *opcode_to_str(const uint16_t opcode) {
    switch (opcode) {
        case HCI_OPCODE_HCI_INQUIRY:                                return "HCI_INQUIRY";
        case HCI_OPCODE_HCI_INQUIRY_CANCEL:                         return "HCI_INQUIRY_CANCEL";
        case HCI_OPCODE_HCI_CREATE_CONNECTION:                      return "HCI_CREATE_CONNECTION";
        case HCI_OPCODE_HCI_ACCEPT_CONNECTION_REQUEST:              return "HCI_ACCEPT_CONNECTION_REQUEST";
        case HCI_OPCODE_HCI_LINK_KEY_REQUEST_REPLY:                 return "HCI_LINK_KEY_REQUEST_REPLY";
        case HCI_OPCODE_HCI_LINK_KEY_REQUEST_NEGATIVE_REPLY:        return "HCI_LINK_KEY_REQUEST_NEGATIVE_REPLY";
        case HCI_OPCODE_HCI_REJECT_CONNECTION_REQUEST:              return "HCI_REJECT_CONNECTION_REQUEST";
        case HCI_OPCODE_HCI_AUTHENTICATION_REQUESTED:               return "HCI_AUTHENTICATION_REQUESTED";
        case HCI_OPCODE_HCI_SET_CONNECTION_ENCRYPTION:              return "HCI_SET_CONNECTION_ENCRYPTION";
        case HCI_OPCODE_HCI_READ_REMOTE_SUPPORTED_FEATURES_COMMAND: return "HCI_READ_REMOTE_SUPPORTED_FEATURES";
        case HCI_OPCODE_HCI_READ_REMOTE_EXTENDED_FEATURES_COMMAND:  return "HCI_READ_REMOTE_EXTENDED_FEATURES";
        case HCI_OPCODE_HCI_SWITCH_ROLE_COMMAND:                    return "HCI_SWITCH_ROLE";
        case HCI_OPCODE_HCI_IO_CAPABILITY_REQUEST_REPLY:            return "HCI_IO_CAPABILITY_REQUEST_REPLY";
        case HCI_OPCODE_HCI_USER_CONFIRMATION_REQUEST_REPLY:        return "HCI_USER_CONFIRMATION_REQUEST_REPLY";
        case HCI_OPCODE_HCI_DISCONNECT:                             return "HCI_DISCONNECT";
        case HCI_OPCODE_HCI_SET_EVENT_MASK:                         return "HCI_SET_EVENT_MASK";
        case HCI_OPCODE_HCI_RESET:                                  return "HCI_RESET";
        case HCI_OPCODE_HCI_WRITE_LOCAL_NAME:                       return "HCI_WRITE_LOCAL_NAME";
        case HCI_OPCODE_HCI_READ_LOCAL_NAME:                        return "HCI_READ_LOCAL_NAME";
        case HCI_OPCODE_HCI_WRITE_PAGE_TIMEOUT:                     return "HCI_WRITE_PAGE_TIMEOUT";
        case HCI_OPCODE_HCI_WRITE_SCAN_ENABLE:                      return "HCI_WRITE_SCAN_ENABLE";
        case HCI_OPCODE_HCI_WRITE_CLASS_OF_DEVICE:                  return "HCI_WRITE_CLASS_OF_DEVICE";
        case HCI_OPCODE_HCI_WRITE_INQUIRY_MODE:                     return "HCI_WRITE_INQUIRY_MODE";
        case HCI_OPCODE_HCI_WRITE_EXTENDED_INQUIRY_RESPONSE:        return "HCI_WRITE_EXTENDED_INQUIRY_RESPONSE";
        case HCI_OPCODE_HCI_WRITE_PAGE_SCAN_TYPE:                   return "HCI_WRITE_PAGE_SCAN_TYPE";
        case HCI_OPCODE_HCI_WRITE_SIMPLE_PAIRING_MODE:              return "HCI_WRITE_SIMPLE_PAIRING_MODE";
        case HCI_OPCODE_HCI_SET_EVENT_MASK_2:                       return "HCI_SET_EVENT_MASK_2";
        case HCI_OPCODE_HCI_WRITE_LE_HOST_SUPPORTED:                return "HCI_WRITE_LE_HOST_SUPPORTED";
        case HCI_OPCODE_HCI_WRITE_SECURE_CONNECTIONS_HOST_SUPPORT:  return "HCI_WRITE_SECURE_CONNECTIONS_HOST_SUPPORT";
        case HCI_OPCODE_HCI_WRITE_DEFAULT_LINK_POLICY_SETTING:      return "HCI_WRITE_DEFAULT_LINK_POLICY_SETTING";
        case HCI_OPCODE_HCI_READ_LOCAL_VERSION_INFORMATION:         return "HCI_READ_LOCAL_VERSION_INFORMATION";
        case HCI_OPCODE_HCI_READ_LOCAL_SUPPORTED_COMMANDS:          return "HCI_READ_LOCAL_SUPPORTED_COMMANDS";
        case HCI_OPCODE_HCI_READ_LOCAL_SUPPORTED_FEATURES:          return "HCI_READ_LOCAL_SUPPORTED_FEATURES";
        case HCI_OPCODE_HCI_READ_BUFFER_SIZE:                       return "HCI_READ_BUFFER_SIZE";
        case HCI_OPCODE_HCI_READ_BD_ADDR:                           return "HCI_READ_BD_ADDR";
        case HCI_OPCODE_HCI_READ_ENCRYPTION_KEY_SIZE:               return "HCI_READ_ENCRYPTION_KEY_SIZE";
        case 0xFC01:                                                 return "HCI_VENDOR_0xFC01";
        default:                                                     return "UNKNOWN_OPCODE";
    }
}

inline constexpr uint32_t crc32_table_entry(uint32_t index) {
    for (unsigned bit = 0; bit < 8; ++bit) {
        index = (index >> 1) ^ (0xEDB88320 & -(index & 1));
    }
    return index;
}

inline constexpr auto make_crc32_table() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < table.size(); ++i) {
        table[i] = crc32_table_entry(i);
    }
    return table;
}

inline constexpr auto crc32_lookup_table = make_crc32_table();

inline uint32_t crc32_seeded(const uint8_t *data, size_t size, const uint32_t seed) {
    uint32_t crc = ~seed;
    while (size--) {
        crc = (crc >> 8) ^ crc32_lookup_table[(crc ^ *data++) & 0xff];
    }
    return ~crc;
}

inline uint32_t crc32(const uint8_t *data, size_t size) {
    return crc32_seeded(data, size, 0xEADA2D49);
}

inline void fill_output_report_checksum(uint8_t *outputData, size_t len) {
    uint32_t crc = crc32(outputData, len - 4);
    outputData[len - 4] = (crc >> 0) & 0xFF;
    outputData[len - 3] = (crc >> 8) & 0xFF;
    outputData[len - 2] = (crc >> 16) & 0xFF;
    outputData[len - 1] = (crc >> 24) & 0xFF;
}

inline uint32_t crc32_feature(const uint8_t *data, size_t size) {
    return crc32_seeded(data, size, 0x2060efc3);
}

inline void fill_feature_report_checksum(uint8_t *data, const size_t len) {
    uint32_t crc = crc32_feature(data, len - 4);
    data[len - 4] = (crc >> 0) & 0xFF;
    data[len - 3] = (crc >> 8) & 0xFF;
    data[len - 2] = (crc >> 16) & 0xFF;
    data[len - 1] = (crc >> 24) & 0xFF;
}

enum PowerState : uint8_t {
    Discharging         = 0x00,
    Charging            = 0x01,
    Complete            = 0x02,
    AbnormalVoltage     = 0x0A,
    AbnormalTemperature = 0x0B,
    ChargingError       = 0x0F
};

enum Direction : uint8_t {
    North = 0, NorthEast, East, SouthEast,
    South, SouthWest, West, NorthWest,
    None = 8
};

struct __attribute__((packed)) TouchFingerData {
    uint32_t Index : 7;
    uint32_t NotTouching : 1;
    uint32_t FingerX : 12;
    uint32_t FingerY : 12;
};

struct __attribute__((packed)) TouchData {
    TouchFingerData Finger[2];
    uint8_t Timestamp;
};

struct __attribute__((packed)) USBGetStateData {
    uint8_t LeftStickX;
    uint8_t LeftStickY;
    uint8_t RightStickX;
    uint8_t RightStickY;
    uint8_t TriggerLeft;
    uint8_t TriggerRight;
    uint8_t SeqNo;
    Direction DPad : 4;
    uint8_t ButtonSquare : 1;
    uint8_t ButtonCross : 1;
    uint8_t ButtonCircle : 1;
    uint8_t ButtonTriangle : 1;
    uint8_t ButtonL1 : 1;
    uint8_t ButtonR1 : 1;
    uint8_t ButtonL2 : 1;
    uint8_t ButtonR2 : 1;
    uint8_t ButtonCreate : 1;
    uint8_t ButtonOptions : 1;
    uint8_t ButtonL3 : 1;
    uint8_t ButtonR3 : 1;
    uint8_t ButtonHome : 1;
    uint8_t ButtonPad : 1;
    uint8_t ButtonMute : 1;
    uint8_t UNK1 : 1;
    uint8_t ButtonLeftFunction : 1;
    uint8_t ButtonRightFunction : 1;
    uint8_t ButtonLeftPaddle : 1;
    uint8_t ButtonRightPaddle : 1;
    uint8_t UNK2;
    uint32_t UNK_COUNTER;
    int16_t AngularVelocityX;
    int16_t AngularVelocityZ;
    int16_t AngularVelocityY;
    int16_t AccelerometerX;
    int16_t AccelerometerY;
    int16_t AccelerometerZ;
    uint32_t SensorTimestamp;
    int8_t Temperature;
    TouchData Touch;
    uint8_t TriggerRightStopLocation : 4;
    uint8_t TriggerRightStatus : 4;
    uint8_t TriggerLeftStopLocation : 4;
    uint8_t TriggerLeftStatus : 4;
    uint32_t HostTimestamp;
    uint8_t TriggerRightEffect : 4;
    uint8_t TriggerLeftEffect : 4;
    uint32_t DeviceTimeStamp;
    uint8_t PowerPercent : 4;
    PowerState Power : 4;
    uint8_t PluggedHeadphones : 1;
    uint8_t PluggedMic : 1;
    uint8_t MicMuted : 1;
    uint8_t PluggedUsbData : 1;
    uint8_t PluggedUsbPower : 1;
    uint8_t UsbPowerOnBT : 1;
    uint8_t DockDetect : 1;
    uint8_t PluggedUnk : 1;
    uint8_t PluggedExternalMic : 1;
    uint8_t HapticLowPassFilter : 1;
    uint8_t PluggedUnk3 : 6;
    uint8_t AesCmac[8];
};

inline void print_hex(const uint8_t *data, size_t size) {
    for (size_t i = 0; i < size; i++) {
        printf("%02x ", data[i]);
    }
    printf("\n");
}

#endif // DS5_BRIDGE_UTILS_H
