#ifndef BTSTACK_CONFIG_H
#define BTSTACK_CONFIG_H

// BTstack features
#define ENABLE_LOG_INFO
#define ENABLE_LOG_ERROR
#define ENABLE_CLASSIC
#define ENABLE_HFP_WIDE_BAND_SPEECH 0
#define ENABLE_SSP_PAIRING
#define ENABLE_L2CAP_ENHANCED_RETRANSMISSION_MODE 0

// Memory configuration
#define MAX_NR_BTSTACK_LINK_KEYS        4
#define MAX_NR_HCI_CONNECTIONS          2
#define MAX_NR_L2CAP_SERVICES           6
#define MAX_NR_L2CAP_CHANNELS           6
#define MAX_NR_RFCOMM_MULTIPLEXERS      0
#define MAX_NR_RFCOMM_SERVICES          0
#define MAX_NR_RFCOMM_CHANNELS          0
#define MAX_NR_SERVICE_RECORD_ITEMS     4

// HCI ACL buffer
#define HCI_ACL_PAYLOAD_SIZE            1024
#define HCI_ACL_CHUNK_SIZE_ALIGNMENT    4
#define MAX_NR_HCI_ACL_PACKETS          8

// L2CAP
#define L2CAP_MINIMAL_MTU               672

// Logging — ENABLE_VERBOSE is 0 or 1 via CMake, use #if not #ifdef
#if ENABLE_VERBOSE
#define ENABLE_LOG_DEBUG
#endif

// HCI transport
#define HAVE_EMBEDDED_TICK
#define HAVE_BTSTACK_STDIN 0

#endif // BTSTACK_CONFIG_H
