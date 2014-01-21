#ifndef PACKETS_H_
#define PACKETS_H_

#include <stdint.h>
#include "sr_interface.h"
#include "sr_common.h"

#define PACKED __attribute__((__packed__))

#define PACKET_CAN_MARSHALL(type, length) (length >= sizeof(type))
#define PACKET_MARSHALL(type, buffer, length) (type *) buffer; buffer += sizeof(type); length -= sizeof(type);

#define ETH_ARP_TYPE (0x806)

#define ARP_HTYPE_ETH (0x1)
#define ARP_PTYPE_IP (0x0800)

#define ARP_OPCODE_REQUEST (1)

typedef struct PACKED packet_Ethernet {
	addr_mac_t dest_mac;
	addr_mac_t source_mac;
	uint16_t type;
} packet_Ethernet_t;

typedef struct PACKED packet_ARP {
	uint16_t hardwaretype;
	uint16_t protocoltype;
	uint8_t hardwareaddresslength;
	uint8_t protocoladdresslength;
	uint16_t opcode;
	addr_mac_t sender_mac;
	addr_ip_t sender_ip;
	addr_mac_t target_mac;
	addr_ip_t target_ip;
} packet_ARP_t;

typedef struct arp_table_entry {
	addr_ip_t ip;
	addr_mac_t mac;
	interface_t* interface;
} arp_table_entry_t;

typedef struct arp_table {
	arp_table_entry_t * entries;
	int entries_size;
} arp_table_t;

typedef struct PACKED packet_IPv4 {
    uint8_t version_ihl;
    uint8_t dscp_ecn;
    uint16_t total_length;
    uint16_t id;
    uint16_t flags_fragmentoffset;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t header_checksum;
    addr_ip_t sourceip;
    addr_ip_t destionationip;
} packet_IPv4_t;

#endif
