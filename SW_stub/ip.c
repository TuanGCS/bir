#include "ip.h"
#include <stdio.h>
#include <netinet/in.h>

#include "sr_interface.h"
#include "sr_integration.h"
#include "globals.h"
#include "ethernet_packet.h"
#include "arp.h"
#include "packets.h"
#include "icmp_type.h"
#include "cli/cli_ping.h"

#include <string.h>

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>

// Move to header file. Should add be unique for every 2^16 - 1 packets then it will overflow and get back to 0 because it is unsigned
volatile uint16_t ip_id = 0;

void ip_print_table(dataqueue_t * table) {

	int i;
	printf("\nTHE IP TABLE\n-------------\n\n");
	for (i = 0; i < table->size; i++) {
		ip_table_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(table, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(ip_table_entry_t));

			printf("%d. IP: %s/%d @ iface %s \n", i,
					quick_ip_to_string(entry->ip), entry->netmask,
					entry->interface->name);

			queue_unlockid(table, i);
		}
	}

	printf("\n");
}

void ip_print(packet_ip4_t * packet) {
	printf("------ IP PACKET -----\n");
	printf("packet->ihl=%d\n", packet->ihl);
	printf("packet->version=%d\n", packet->version);
	printf("packet->dscp_ecn=%d\n", packet->dscp_ecn);
	printf("packet->total_length=htons(%d)\n", ntohs(packet->total_length));
	printf("packet->id=htons(%d)\n", ntohs(packet->id));
	printf("packet->flags_fragmentoffset=htons(%d)\n",
			ntohs(packet->flags_fragmentoffset));
	printf("packet->ttl=%d\n", packet->ttl);
	printf("packet->protocol=%d\n", packet->protocol);
	printf("packet->header_checksum=htons(%d)\n",
			ntohs(packet->header_checksum));
	printf("packet->src_ip=\"%s\"\n", quick_ip_to_string(packet->src_ip));
	printf("packet->dst_ip=\"%s\"\n", quick_ip_to_string(packet->dst_ip));
	printf("\n");
	fflush(stdout);
}

// TODO! what happens if two interfaces match?
// returns id in table
int ip_longestprefixmatch(dataqueue_t * table, addr_ip_t ip,
		ip_table_entry_t * result) {

	int maxmatch = 0;
	int answer = -1;

	int i;
	ip_table_entry_t * entry;

	int entry_size;
	for (i = 0; i < table->size; i++) {
		if (queue_getidandlock(table, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(ip_table_entry_t));

			uint64_t mask = (1 >> (entry->netmask + 1)) - 1;

			const uint32_t tableip = entry->ip;

			if ((ip & mask) == (tableip & mask)) {
				if (maxmatch < entry->netmask) {
					maxmatch = entry->netmask;
					answer = i;
					*result = *entry;
				}
			}

			queue_unlockid(table, i);
		}
	}

	return answer;
}

// returns id in table
int ip_directmatch(dataqueue_t * table, addr_ip_t ip, ip_table_entry_t * result) {
	int i;
	for (i = 0; i < table->size; i++) {
		ip_table_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(table, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(ip_table_entry_t));

			if (entry->ip == ip) {
				*result = *entry;
				queue_unlockid(table, i);
				return i;
			} else
				queue_unlockid(table, i);
		}
	}

	return -1;
}

void ip_putintable(dataqueue_t * table, addr_ip_t ip, interface_t* interface,
		int netmask, bool dynamic) {

	ip_table_entry_t existing; // memory allocation ;(

	int id = ip_directmatch(table, ip, &existing);
	if (id >= 0) {
		if (existing.interface == interface)
			return; // nothing to do, this already exists
		else
			fprintf(stderr,
					"Possible IP conflict! %s was already registered at %s but now there is another %s that wants to be registered on %s!\n",
					quick_ip_to_string(existing.ip), existing.interface->name,
					quick_ip_to_string(ip), interface->name);
	}

	existing.interface = interface;
	existing.ip = ip;
	existing.netmask = netmask;
	existing.dynamic = dynamic;

	queue_add(table, (void *) &existing, sizeof(ip_table_entry_t));

	// for debug TODO remove after use
	ip_print_table(table);
}

void sr_transport_input(uint8_t* packet /* borrowed */); // this function does the transport input to the system

void update_ip_packet_response(packet_info_t* pi, addr_ip_t dst_ip,
		addr_ip_t src_ip, uint8_t ttl) {

	packet_ip4_t* ipv4 = (packet_ip4_t *) &pi->packet[sizeof(packet_ethernet_t)];

	ipv4->dst_ip = dst_ip;
	ipv4->src_ip = src_ip;

	ipv4->ttl = ttl;
	ipv4->flags_fragmentoffset = 0;
	ipv4->id = htons(ip_id); // Fix
	ip_id++;

	ipv4->header_checksum = 0;
	ipv4->header_checksum = generatechecksum((unsigned short*) ipv4,
			sizeof(packet_ip4_t));

	packet_ethernet_t* eth = (packet_ethernet_t *) pi->packet;
	update_ethernet_header(pi, eth->source_mac, eth->dest_mac);
}

bool ip_header_check(packet_info_t* pi, packet_ip4_t * ipv4) {

	// Check if IP packet is verion 4
	if (ipv4->version != 4) {
		fprintf(stderr, "Not a IPv4 packet\n");
		return FALSE;
	}

	// Check if packet contains options
	if (ipv4->ihl != sizeof(packet_ip4_t) >> 2) {
		fprintf(stderr, "IPv4 packet contains options\n");
		return FALSE;
	}

	// Check if packet length is larger than the specified total length
	if (ipv4->total_length < sizeof(pi->packet[sizeof(packet_ethernet_t)])) {
		fprintf(stderr,
				"Invalid length of packet (received %lu, expected %d)\n",
				sizeof(pi->packet[sizeof(packet_ethernet_t)]),
				ipv4->total_length);
		return FALSE;
	}

	// Calculate and check header checksum
	if (generatechecksum((unsigned short*) ipv4, sizeof(packet_ip4_t)) != (0)) {
		fprintf(stderr, "IPv4 Header Checksum Error\n");
		return FALSE;
	}

	// Check if packet is a fragment -> Tested
	if ((ntohs(ipv4->flags_fragmentoffset) & 0x3FFF) != 0) {
		fprintf(stderr, "Packet is a fragment\n");
		return FALSE;
	}

	// Check if packet contains any options TODO

	ipv4->ttl--;
	if (ipv4->ttl < 1) {
		fprintf(stderr, "Packet Time-To-Live is 0 or less\n");

		icmp_type_time_exceeded(pi, ipv4);

		return FALSE;
	}

	return TRUE;

}

void ip_onreceive(packet_info_t* pi, packet_ip4_t * ipv4) {

	// Check the validity of the IP header
	if (!ip_header_check(pi, ipv4)) {
		fprintf(stderr, "Invalid IP received (wrong checksum)\n");
		return;
	}

	int i;
	for (i = 0; i < pi->router->num_interfaces; i++) {

		if (ipv4->dst_ip == pi->router->interface[i].ip) {

			if (ipv4->protocol == IP_TYPE_ICMP) {

				packet_icmp_t* icmp =
						(packet_icmp_t *) &pi->packet[sizeof(packet_ethernet_t)
								+ sizeof(packet_ip4_t)];

				if (icmp->type == ICMP_TYPE_REQUEST) {
					icmp_type_echo_replay(pi, icmp);
					return;

				} else if (icmp->type == ICMP_TYPE_REPLAY) {
					cli_ping_handle_reply(ipv4->src_ip, icmp->seq_num);
					return;
				}

			} else if (ipv4->protocol == IP_TYPE_TCP) {

				sr_transport_input((uint8_t *) ipv4);
				return;

			} else {

				fprintf(stderr, "Unsupported IP packet type \n");
				icmp_type_dst_unreach(pi, ipv4, ICMP_CODE_PROT_UNREACH);
				return;

			}

		}
	}

	ip_table_entry_t dest_ip_entry; // memory allocation ;(

	if (ip_longestprefixmatch(&pi->router->ip_table, ipv4->dst_ip,
			&dest_ip_entry) >= 0) {

		arp_cache_entry_t arp_dest; // memory allocation ;(

		if (arp_getcachebyip(&pi->router->arp_cache, ipv4->dst_ip, &arp_dest)
				>= 0) {

			ipv4->header_checksum = 0;
			ipv4->header_checksum = generatechecksum((unsigned short*) ipv4,
					sizeof(packet_ip4_t));

			if (ipv4->ttl < 1)
				return;

			ethernet_packet_send(get_sr(), dest_ip_entry.interface,
					arp_dest.mac, dest_ip_entry.interface->mac,

					htons(ETH_IP_TYPE), pi);
		} else {
			fprintf(stderr,
					"IP packet will be queued upon ARP request response.\n");

			// add to queue
			byte data[sizeof(packet_info_t) + pi->len];
			memcpy(data, (void *) pi, sizeof(packet_info_t)); // first add packet info
			memcpy(&data[sizeof(packet_info_t)], (void *) pi->packet, pi->len); // then add the data intself

			// TODO! what if we start pinging an unknown address until memory runs out? how do we flush iparp_buffer?
			queue_add(&pi->router->iparp_buffer, (void *) &data,
					sizeof(packet_info_t) + pi->len); // add current packet to queue

			arp_send_request(pi->router, dest_ip_entry.interface, ipv4->dst_ip);

		}

	} else {
		printf("Longest prefix matching failed to find an interface for %s\n",
				quick_ip_to_string(ipv4->dst_ip));
		ip_print_table(&pi->router->ip_table);

		icmp_type_dst_unreach(pi, ipv4, ICMP_CODE_HOST_UNREACH);
	}

}
