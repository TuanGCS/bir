
#ifndef DNS_H_
#define DNS_H_

typedef struct dns_query_ho {
	char ** query_names;
	int count;
	uint16_t qtype;
	uint16_t qclass;
} dns_query_ho_t;

void dns_onreceive(packet_info_t* pi, packet_udp_t * udp, packet_dns_t * dns);

#endif /* DNS_H_ */
