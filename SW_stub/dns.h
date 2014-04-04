
#ifndef DNS_H_
#define DNS_H_

#define DNS_TYPE_A (0)
#define DNS_TYPE_NS (1)

typedef struct dns_query_ho {
	char ** query_names;
	int count;
	uint16_t qtype;
	uint16_t qclass;
} dns_query_ho_t;


typedef struct dns_cache_entry {
	char ** names;
	int count;
	uint16_t type;
	uint16_t class;
	uint32_t ttl;
	uint16_t length;
	byte * rdata;
} dns_cache_entry_t;

typedef struct dns_db_entry {
	char ** query_names;
	uint16_t type;
	uint16_t class;
	uint16_t length;
	byte * rdata;
} dns_db_entry_t;

typedef struct dns_r_proto_packet {
	byte * packet;
	int totalsize;
} dns_answer_proto_packet_t;


void dns_onreceive(packet_info_t* pi, packet_udp_t * udp, packet_dns_t * dns);

#endif /* DNS_H_ */
