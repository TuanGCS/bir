#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "icmp_type.h"
#include "globals.h"
#include "ip.h"

#include "stdlib.h"
#include "sr_integration.h"

void icmp_type_echo_replay(packet_info_t* pi, packet_icmp_t* icmp) {

	icmp->type = ICMP_TYPE_REPLAY;

	icmp->header_checksum = 0;
	icmp->header_checksum = generatechecksum((unsigned short*) icmp,
			sizeof(packet_icmp_t));

	packet_ip4_t* ipv4 = (packet_ip4_t *) &pi->packet[sizeof(packet_ethernet_t)];
	update_ip_packet_response(pi, ipv4->src_ip, ipv4->dst_ip, ipv4->ttl);
}

void icmp_type_dst_unreach(packet_info_t* pi, packet_ip4_t* ipv4, int code) {

	packet_icmp_t* icmp =
			(packet_icmp_t *) &pi->packet[sizeof(packet_ethernet_t)
					+ sizeof(packet_ip4_t)];

	ipv4->ttl++;
	byte data[28];
	memcpy(data, (void *) ipv4, 20);
	memcpy(&data[20], (void *) icmp, 8);

	icmp->type = ICMP_TYPE_DST_UNREACH;
	icmp->code = code;
	icmp->id = 0;
	if (icmp->code == (ICMP_CODE_DG_BIG)) {
		icmp->seq_num = 1500; // TODO
	}

	memcpy(icmp->data, data, 28);

	icmp->header_checksum = 0;
	icmp->header_checksum = generatechecksum((unsigned short*) icmp,
			sizeof(packet_icmp_t));

	update_ip_packet_response(pi, ipv4->src_ip, pi->interface->ip, ipv4->ttl);

}

void icmp_type_time_exceeded(packet_info_t* pi, packet_ip4_t* ipv4, addr_ip_t src) {

	packet_icmp_t* icmp =
			(packet_icmp_t *) &pi->packet[sizeof(packet_ethernet_t)
					+ sizeof(packet_ip4_t)];

	byte data[28];
	memcpy(data, (void *) ipv4, 20);
	memcpy(&data[20], (void *) icmp, 8);

	icmp_allocate_and_send(get_router(), ipv4->src_ip, ICMP_TYPE_TIME_EXCEEDED, 0, 0, 0, data, 56, src);

}

int icmp_allocate_and_send(router_t * rtr, addr_ip_t ip, int code, int type,
		int id, int count, byte * data, int data_size, addr_ip_t src) {

	packet_info_t * pi;
	if (!packetinfo_ip_allocate(get_router(), &pi,
			sizeof(packet_ethernet_t) + sizeof(packet_ip4_t)
					+ sizeof(packet_icmp_t), ip, src, IP_TYPE_ICMP, sizeof(packet_ip4_t) + 8 + data_size))
		return 0;

	pi->len = sizeof(packet_ethernet_t) + sizeof(packet_ip4_t) + 8 + data_size;

	packet_icmp_t* icmp = PACKET_MARSHALL(packet_icmp_t, pi->packet,
			sizeof(packet_ethernet_t) + sizeof(packet_ip4_t));

	icmp->code = code;
	icmp->id = htons(id);
	icmp->type = type;
	icmp->seq_num = htons(count);
	if (data_size > 0)
		memcpy(icmp->data, data, data_size);

	icmp->header_checksum = 0;
	icmp->header_checksum = generatechecksum((unsigned short*) icmp,
			8+data_size);

	ip_onreceive(pi,
			PACKET_MARSHALL(packet_ip4_t, pi->packet,
					sizeof(packet_ethernet_t)));

	// free memory
	packetinfo_free(&pi);

	return 1;
}

