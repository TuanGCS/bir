#include "globals.h"
#include "stdlib.h"
#include "string.h"

int generatechecksum(unsigned short * buf, int len) {

	uint16_t * data = (uint16_t *) buf;
	int size = len / 2;

	uint32_t sum;
	for (sum = 0; size > 0; size--)
		sum += ntohs(*data++);

	if ( len & 1 )
		sum += (*((uint8_t *)data)) << 8;

	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return htons(((uint16_t) ~sum));
}

byte * malloc_udp_pseudo_header(packet_udp_t * udp, packet_ip4_t * ip, int * size) {

	typedef struct PACKED {
		addr_ip_t srcip;
		addr_ip_t dstip;
		uint8_t zeroes;
		uint8_t protocol;
		uint16_t UDP_length;
		uint16_t source_port;
		uint16_t destination_port;
		uint16_t length;
		uint16_t checksum;
	} packet_udp_pseudo_header_t;

	const int datasize = ntohs(udp->length) - sizeof(packet_udp_t);
	*size = sizeof(packet_udp_pseudo_header_t) + datasize;
	byte * res = malloc(*size);

	packet_udp_pseudo_header_t * dest = (packet_udp_pseudo_header_t *) res;

	dest->srcip = ip->src_ip;
	dest->dstip = ip->dst_ip;
	dest->zeroes = 0;
	dest->protocol = ip->protocol;

	dest->UDP_length = udp->length;
	dest->source_port = udp->source_port;
	dest->destination_port = udp->destination_port;
	dest->length = udp->length;
	dest->checksum = udp->checksum;

	memcpy(&res[sizeof(packet_udp_pseudo_header_t)], ((byte *) udp) + sizeof(packet_udp_t), datasize);

	return res;
}
