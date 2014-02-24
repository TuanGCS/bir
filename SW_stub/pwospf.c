#include <stdio.h>
#include <stdlib.h>

#include "pwospf.h"
#include "sr_router.h"
#include "packets.h"
#include "globals.h"
#include "dataqueue.h"

volatile uint16_t hid = 0;

void pwospf_print(pwospf_packet_t * packet) {
	printf("------ PWOSPF (Pee-Wee Open Shortest Path First) PACKET -----\n");
	printf("packet->area_id=htonl(%d) //htonl(0x%x)\n", ntohl(packet->area_id), ntohl(packet->area_id));
	printf("packet->auth_data=htonl(%d) //htonl(0x%x)\n", ntohl(packet->auth_data), ntohl(packet->auth_data));
	printf("packet->auth_type=htonl(%d) //htonl(0x%x)\n", ntohl(packet->auth_type), ntohl(packet->auth_type));
	printf("packet->autotype=htons(%d) //htons(0x%x)\n", ntohs(packet->autotype), ntohs(packet->autotype));
	printf("packet->checksum=htons(%d) //htons(0x%x)\n", ntohs(packet->checksum), ntohs(packet->checksum));
	printf("packet->len=htons(%d)\n", ntohs(packet->len));
	printf("packet->router_id=%s //htonl(0x%x)\n", quick_ip_to_string(packet->router_id), ntohl(packet->router_id));
	printf("packet->type=%d //0x%x\n", packet->type, packet->type);
	printf("packet->version=%d //0x%x\n", packet->version, packet->version);
	printf("\n");
	fflush(stdout);
}

void pwospf_hello_print(pwospf_packet_hello_t * packet) {
	pwospf_print(&packet->pwospf_header);

	printf("****** HELLO part ******\n");
	printf("packet->net_mask=%s //htonl(0x%x)\n", quick_ip_to_string(packet->netmask), ntohl(packet->netmask));
	printf("packet->helloint=htons(%d)\n", ntohs(packet->helloint));
	printf("packet->padding=htons(%d) //htons(0x%x)\n", ntohs(packet->padding), ntohs(packet->padding));
	printf("\n");
	fflush(stdout);
}

void pwospf_onreceive_hello(packet_info_t * pi, pwospf_packet_hello_t * packet) {
	interface_t * interface = pi->interface;
	dataqueue_t * neighbours = &interface->neighbours;
	packet_ip4_t * ip = PACKET_MARSHALL(packet_ip4_t, pi->packet, sizeof(packet_ethernet_t));

	int i;
	for (i = 0; i < neighbours->size; i++) {
		pwospf_list_entry_t * entry;
		int entry_size;
		int discovered = 0;
		if (queue_getidandlock(neighbours, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(pwospf_list_entry_t));

			if (entry->neighbour_id == packet->pwospf_header.router_id) {
				discovered = 1;

				if (entry->neighbour_ip != ip->src_ip) {
					debug_println("OSPF: Router id %d has a new IP address of %s :)", packet->pwospf_header.router_id, quick_ip_to_string(ip->src_ip));
					entry->neighbour_ip = ip->src_ip;
				}

				gettimeofday(&entry->timestamp, NULL);
			}

			queue_unlockid(neighbours, i);
		}
		if (discovered) {
			debug_println("OSPF: Router id %d said HELLO :)", packet->pwospf_header.router_id);
			return;
		}
	}

	// this is a new neighbour, add it to the list!
	pwospf_list_entry_t newneighbour;
	newneighbour.neighbour_id = packet->pwospf_header.router_id;
	newneighbour.neighbour_ip = ip->src_ip;
	gettimeofday(&newneighbour.timestamp, NULL);

	queue_add(neighbours, &newneighbour, sizeof(pwospf_list_entry_t));
	debug_println("OSPF: A new neighbour was discovered on interface %s; Router id %d with ip %s.", pi->interface->name, newneighbour.neighbour_id, quick_ip_to_string(newneighbour.neighbour_ip));
}

void pwospf_onreceive(packet_info_t* pi, pwospf_packet_t * packet) {
	switch(packet->type) {
	case OSPF_TYPE_HELLO:
		if (PACKET_CAN_MARSHALL(pwospf_packet_hello_t, sizeof(packet_ethernet_t)+sizeof(packet_ip4_t), pi->len)) {
			pwospf_packet_hello_t * pwospfhello = PACKET_MARSHALL(pwospf_packet_hello_t,pi->packet, sizeof(packet_ethernet_t)+sizeof(packet_ip4_t));
			pwospf_onreceive_hello(pi, pwospfhello);
		} else
			fprintf(stderr, "Invalid PWOSPF HELLO packet!\n");
		break;
	default:
		fprintf(stderr, "Unsupported PWOSPF type 0x%x\n", packet->type);
		break;
	}
}

pwospf_packet_hello_t* generate_pwospf_hello_header(addr_ip_t rid,
		addr_ip_t aid, uint32_t netmask) {

	pwospf_packet_hello_t* pw_hello = (pwospf_packet_hello_t *) malloc(
			sizeof(pwospf_packet_hello_t));

	pw_hello->pwospf_header.version = OSPF_VERSION;
	pw_hello->pwospf_header.type = OSPF_TYPE_HELLO;
	pw_hello->pwospf_header.len = sizeof(pwospf_packet_t);
	pw_hello->pwospf_header.router_id = rid;
	pw_hello->pwospf_header.area_id = aid;
	pw_hello->pwospf_header.autotype = 0;
	pw_hello->pwospf_header.auth_type = 0;
	pw_hello->pwospf_header.auth_data = 0;
	pw_hello->netmask = netmask;
	pw_hello->helloint = HELLOINT;
	pw_hello->padding = 0;

	pw_hello->pwospf_header.checksum = 0;
	pw_hello->pwospf_header.checksum = generatechecksum(
			(unsigned short*) pw_hello, sizeof(pwospf_packet_t) - 8);

	return pw_hello;

}

pwospf_packet_link_t* generate_pwospf_link_header(addr_ip_t rid, addr_ip_t aid,
		uint32_t netmask, uint16_t advert) {

	pwospf_packet_link_t* pw_link = (pwospf_packet_link_t *) malloc(
			sizeof(pwospf_packet_link_t));

	pw_link->pwospf_header.version = OSPF_VERSION;
	pw_link->pwospf_header.type = OSPF_TYPE_HELLO;
	pw_link->pwospf_header.len = sizeof(pwospf_packet_t);
	pw_link->pwospf_header.router_id = rid;
	pw_link->pwospf_header.area_id = aid;
	pw_link->pwospf_header.autotype = 0;
	pw_link->pwospf_header.auth_type = 0;
	pw_link->pwospf_header.auth_data = 0;
	pw_link->seq = hid;
	hid++;
	pw_link->ttl = 64;
	pw_link->advert = advert;

	pw_link->pwospf_header.checksum = 0;
	pw_link->pwospf_header.checksum = generatechecksum(
			(unsigned short*) pw_link, sizeof(pwospf_packet_t) - 8);

	return pw_link;

}

pwospf_lsa_t* generate_pwospf_lsa(pwospf_router_t pw_router, uint16_t advert) {

	pwospf_lsa_t lsa[advert];
	memset(lsa, 0, advert * sizeof(pwospf_lsa_t));

	int i, j;
	for (i = 0; i < 10; i++) { // TODO



		for (i = 0; i < advert; i++) {
			lsa[advert].subnet = 0;
			lsa[advert].netmask = 0;
			lsa[advert].router_id = 0;
		}
	}

	return lsa;

}
