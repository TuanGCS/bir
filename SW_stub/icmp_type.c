#include <string.h>

#include "icmp_type.h"
#include "globals.h"
#include "ip.h"

void icmp_type_echo_replay(packet_info_t* pi, packet_icmp_t* icmp) {

	icmp->type = ICMP_TYPE_REPLAY;

	icmp->header_checksum = 0;
	icmp->header_checksum = generatechecksum((unsigned short*) icmp,
			sizeof(packet_icmp_t));

	packet_ip4_t* ipv4 = (packet_ip4_t *) &pi->packet[sizeof(packet_ethernet_t)];
	update_ip_packet_response(pi, ipv4->src_ip, ipv4->dst_ip, ipv4->ttl);
}

void icmp_type_prot_supp(packet_info_t* pi, packet_ip4_t* ipv4) {

	// TODO

}

void icmp_type_dst_unreach(packet_info_t* pi, packet_ip4_t* ipv4) {

	packet_icmp_t* icmp =
				(packet_icmp_t *) &pi->packet[sizeof(packet_ethernet_t)
						+ sizeof(packet_ip4_t)];

	ipv4->ttl++;
	byte data[28];
	memcpy(data, (void *) ipv4, 20);
	memcpy(&data[20], (void *) icmp, 8);

	icmp->type = ICMP_TYPE_DST_UNREACH;
	icmp->code = ICMP_CODE_HOST_UNREACH;
	icmp->id = 0;
	if (icmp->code == (ICMP_CODE_DG_BIG)) {
		icmp->seq_num = 1500; // Change
	}

	memcpy(icmp->data, data, 28);

	icmp->header_checksum = 0;
	icmp->header_checksum = generatechecksum((unsigned short*) icmp,
			sizeof(packet_icmp_t));

	update_ip_packet_response(pi, ipv4->src_ip, ipv4->dst_ip, ipv4->ttl);

}

void icmp_type_time_exceeded(packet_info_t* pi, packet_ip4_t* ipv4) {

	packet_icmp_t* icmp =
			(packet_icmp_t *) &pi->packet[sizeof(packet_ethernet_t)
					+ sizeof(packet_ip4_t)];

	ipv4->ttl++;
	byte data[28];
	memcpy(data, (void *) ipv4, sizeof(packet_ip4_t));
	memcpy(&data[20], (void *) icmp, 8);

	icmp->type = ICMP_TYPE_TIME_EXCEEDED;
	icmp->code = 0;

	icmp->id = 0;
	icmp->seq_num = 0;

	memcpy(icmp->data, data, 28);

	icmp->header_checksum = 0;
	icmp->header_checksum = generatechecksum((unsigned short*) icmp,
			sizeof(packet_icmp_t));

	update_ip_packet_response(pi, ipv4->src_ip, pi->interface->ip, 64);

}

