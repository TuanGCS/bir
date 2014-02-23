#ifndef PACKETS_H_
#define PACKETS_H_

#include <stdint.h>
#include "sr_interface.h"
#include "sr_common.h"

#define PACKED __attribute__((__packed__))

#define PACKET_CAN_MARSHALL(type, offset, len) ((len - offset) >= sizeof(type))
#define PACKET_MARSHALL(type, s_ptr, offset) (type *) &s_ptr[offset]

#define ETH_IP_TYPE (0x0800)
#define ETH_ARP_TYPE (0x0806)
#define ETH_RARP_TYPE (0x8035)



#define PROTOCOL_TCP (6)

#define MAC_BROADCAST {.octet = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}}

#define ICMP_TYPE_REPLAY (0)
#define ICMP_TYPE_DST_UNREACH (3)
#define ICMP_TYPE_REQUEST (8)
#define ICMP_TYPE_TIME_EXCEEDED (11)
#define ICMP_TYPE_TRACEROUTE (30)

#define IP_TYPE_ICMP (1)

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
	uint16_t seq_num;
	byte data[56];
} packet_icmp_t;

typedef struct arp_cache_entry {
	addr_ip_t ip;
	addr_mac_t mac;
	struct timeval tv;
} arp_cache_entry_t;

typedef struct PACKED packet_ip4 {
	unsigned ihl:4;
	unsigned version:4;
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
	int netmask;
} ip_table_entry_t;

#endif
