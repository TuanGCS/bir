#ifndef PACKETS_H_
#define PACKETS_H_

#include <stdint.h>
#include <sys/time.h>
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
#define MAC_ZERO {.octet = {0,0,0,0,0,0}}

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

typedef struct PACKED packet_tcp {
	uint16_t source_port;
	uint16_t destination_port;
	uint32_t sequence_number;
	uint32_t ACK_number;
	unsigned offset:4;
	unsigned reserved:3;
	unsigned NS:1;
	unsigned CWR:1;
	unsigned ECE:1;
	unsigned URG:1;
	unsigned ACK:1;
	unsigned PSH:1;
	unsigned RST:1;
	unsigned SYN:1;
	unsigned FIN:1;
	uint16_t window_size;
	uint16_t checksum;
	uint16_t urgent_pointer;
} packet_tcp_t;

typedef struct PACKED packet_udp {
	uint16_t source_port;
	uint16_t destination_port;
	uint16_t length;
	uint16_t checksum;
} packet_udp_t;

typedef struct PACKED packet_dns {
	uint16_t id;
	unsigned QR:1;
	unsigned Opcode:4;
	unsigned AA:1;
	unsigned TC:1;
	unsigned RD:1;
	unsigned RA:1;
	unsigned Z:1;
	unsigned AD:1;
	unsigned CD:1;
	unsigned Rcode:4;
	uint16_t totalquestions;
	uint16_t totalanswerrrs;
	uint16_t totalauthorityrrs;
	uint16_t totaladditionalrrs;
} packet_dns_t;

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

typedef struct rtable_entry {
	addr_ip_t router_ip;
	addr_ip_t subnet;
	addr_ip_t netmask;
	interface_t* interface;
	int metric;
	bool dynamic;
} rtable_entry_t;

typedef struct PACKED pwospf_packet {
	uint8_t version;
	uint8_t type;
	uint16_t len;
	uint32_t router_id;
	uint32_t area_id;
	uint16_t checksum;
	uint16_t autotype;
	uint32_t auth_type;
	uint32_t auth_data;
} pwospf_packet_t;

typedef struct PACKED pwospf_packet_hello {
	pwospf_packet_t pwospf_header;
	uint32_t netmask;
	uint16_t helloint;
	uint16_t padding;
} pwospf_packet_hello_t;

typedef struct PACKED pwospf_packet_link {
	pwospf_packet_t pwospf_header;
	uint16_t seq;
	uint16_t ttl;
	uint32_t advert;
} pwospf_packet_link_t;

typedef struct pwospf_router {
	uint32_t router_id;
	uint32_t area_id;
	uint16_t lsuint;
} pwospf_router_t;

typedef struct PACKED pwospf_lsa {
	uint32_t subnet;
	uint32_t netmask;
	uint32_t router_id;
} pwospf_lsa_t;

typedef struct PACKED pwospf_lsa_dj {
	pwospf_lsa_t lsa;
	addr_ip_t router_ip;
} pwospf_lsa_dj_t;

typedef struct pwospf_list_entry {
	uint32_t neighbour_id;
	addr_ip_t neighbour_ip;
	uint16_t helloint;
	struct timeval timestamp;

	int immediate_neighbour;

	// for LSU
	uint16_t lsu_lastseq;
	pwospf_lsa_t * lsu_lastcontents;
	int lsu_lastcontents_count;
	struct timeval lsu_timestamp;
} pwospf_list_entry_t;

typedef struct iparp_buffer_entry {
	byte * packet_info;
	struct timeval reception_time;
	int sent_arps_count;
	int len;
	interface_t * intf;
	addr_ip_t dest;
} iparp_buffer_entry_t;

typedef struct PACKED d_link {
	uint32_t subnet;
	uint32_t netmask;
} d_link_t;

typedef struct PACKED djikstra_entry {
	interface_t* intf;
	int metric;
} djikstra_entry_t;

#endif
