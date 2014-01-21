#ifndef PACKETS_H_
#define PACKETS_H_

#include <stdint.h>
#include "sr_common.h"

#define PACKED __attribute__((__packed__))

#define PACKET_CAN_MARSHALL(type, length) (length >= sizeof(type))
#define PACKET_MARSHALL(type, buffer, length) (type *) buffer; buffer += sizeof(type); length -= sizeof(type);

#define ETH_ARP_TYPE (0x806)

#define ARP_HTYPE_ETH_NO (0x0100)
#define ARP_PTYPE_IP_NO (0x8)

#define ARP_OPCODE_REQUEST (1)

typedef struct PACKED packet_Ethernet {
	uint8_t dest_mac[6];
	uint8_t source_mac[6];
	uint16_t type;
} packet_Ethernet_t;

typedef struct PACKED packet_ARP {
	uint16_t hardwaretype;
	uint16_t protocoltype;
	uint8_t hardwareaddresslength;
	uint8_t protocoladdresslength;
	uint16_t opcode;
	uint8_t sender_mac[6];
	uint8_t sender_ip[4];
	uint8_t target_mac[6];
	uint8_t target_ip[4];
} packet_ARP_t;

typedef struct PACKED packet_IPv4 {
    uint8_t version_ihl;
    uint8_t dscp_ecn;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_fragmentoffset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t header_checksum;
    uint8_t sourceip[4];
    uint8_t destionationip[4];
} packet_IPv4_t;

#endif
