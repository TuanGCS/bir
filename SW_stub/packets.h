#ifndef PACKETS_H_
#define PACKETS_H_

#include <stdint.h>
#include "sr_interface.h"
#include "sr_common.h"

#define PACKED __attribute__((__packed__))

#define PACKET_CAN_MARSHALL(type, offset, len) ((len - offset) >= sizeof(type))
#define PACKET_MARSHALL(type, s_ptr, offset) (type *) (void *) &s_ptr[offset]

#define ETH_IP_TYPE (0x0800)
#define ETH_ARP_TYPE (0x0806)
#define ETH_RARP_TYPE (0x8035)

#define ARP_HTYPE_ETH (0x1)
#define ARP_PTYPE_IP (0x0800)

#define ARP_OPCODE_REQUEST (1)
#define ARP_OPCODE_REPLAY (2)

#define ARP_CACHE_TIMEOUT_REQUEST (120)
#define ARP_CACHE_TIMEOUT_BROADCAST (60)
#define ARP_CACHE_TIMEOUT_STATIC (-1)

#define ARP_THRESHOLD (50)

#define MAC_BROADCAST (.octet = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF})

#define ICMP_TYPE_REQUEST (8)
#define ICMP_TYPE_REPLAY (0)

typedef struct PACKED packet_ethernet {
	addr_mac_t dest_mac;
	addr_mac_t source_mac;
	uint16_t type;
} packet_ethernet_t;

typedef struct PACKED packet_arp {
	uint16_t hardwaretype;
	uint16_t protocoltype;
	uint8_t hardwareaddresslength;
	uint8_t protocoladdresslength;
	uint16_t opcode;
	addr_mac_t sender_mac;
	addr_ip_t sender_ip;
	addr_mac_t target_mac;
	addr_ip_t target_ip;
} packet_arp_t;

typedef struct PACKED packet_icmp {
	uint8_t type;
	uint8_t code;
	uint16_t header_checksum;
	uint16_t id;
	uint16_t seqNum;
	char data[56];
} packet_icmp_t;

typedef struct arp_cache_entry {
	addr_ip_t ip;
	addr_mac_t mac;
	interface_t* interface;
	struct timeval tv;
} arp_cache_entry_t;

typedef struct PACKED packet_ip4 {
	uint8_t version_ihl;
	uint8_t dscp_ecn;
	uint16_t total_length;
	uint16_t id;
	uint16_t flags_fragmentoffset;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t header_checksum;
	addr_ip_t src_ip;
	addr_ip_t dst_ip;
} packet_ip4_t;

typedef struct ip_table_entry {
	addr_ip_t ip;
	interface_t* interface;
} ip_table_entry_t;

#endif
