#include "ip.h"
#include <stdio.h>
#include <netinet/in.h>

#include "sr_interface.h"
#include "sr_integration.h"
#include "ethernet_packet.h"
#include "arp.h"
#include "packets.h"
#include <string.h>

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>

volatile uint16_t id = 5892;

void ip_print_table(dataqueue_t * table) {

	int i;
	printf("THE IP TABLE\n-------------\n\n");
	for (i = 0; i < table->size; i++) {
		ip_table_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(table, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(ip_table_entry_t));

			printf("%d. IP: %s @ iface %s\n", i, quick_ip_to_string(entry->ip),
					entry->interface->name);

			queue_unlockid(table, i);
		}
	}

	printf("\n");
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

			const uint32_t tableip = entry->ip;

			if (ip == tableip) {
				// we won't find a better match
				*result = *entry;
				queue_unlockid(table, i);
				return i;
			} else if ((ip & 0xFFFFFF) == (tableip & 0xFFFFFF)) {
				if (maxmatch < 3) {
					maxmatch = 3;
					answer = i;
				}
			} else if ((ip & 0xFFFF) == (tableip & 0xFFFF)) {
				if (maxmatch < 2) {
					maxmatch = 2;
					answer = i;
				}
			} else if ((ip & 0xFF) == (tableip & 0xFF)) {
				if (maxmatch < 1) {
					maxmatch = 1;
					answer = i;
				}
			}

			queue_unlockid(table, i);
		}
	}

	if (queue_getidandlock(table, answer, (void **) &entry, &entry_size)) {
		*result = *entry;
		queue_unlockid(table, answer);
		return answer;
	}

	return -1;
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

void ip_putintable(dataqueue_t * table, addr_ip_t ip, interface_t* interface) {

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

	queue_add(table, (void *) &existing, sizeof(ip_table_entry_t));

	// for debug TODO remove after use
	ip_print_table(table);
}

static inline int ip_checkchecksum(packet_ip4_t * ipv4) {
	uint16_t * data = (uint16_t *) ipv4;
	int size = sizeof(packet_ip4_t) / 2;

	uint32_t sum;
	for (sum = 0; size > 0; size--)
		sum += ntohs(*data++);
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return ((uint16_t) ~sum) == (0);
}

static inline int generatechecksum(unsigned short * buf, int len) {

	uint16_t * data = (uint16_t *) buf;
	int size = len / 2;

	uint32_t sum;
	for (sum = 0; size > 0; size--)
		sum += ntohs(*data++);
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return htons(((uint16_t) ~sum));
}

void ip_onreceive(packet_info_t* pi, packet_ip4_t * ipv4) {

	// TODO! check ipv4 version, flags, header size, etc.

	if (!ip_checkchecksum(ipv4)) {
		fprintf(stderr, "IPv4 CHECKSUM ERR\n");
		return;
	}

	if (ipv4->ttl < 1)
		return; // don't handle packets with ttl less than 1 TODO should that be less than 0

	int i;
	for (i = 0; i < pi->router->num_interfaces; i++) {
		if (ipv4->dst_ip == pi->router->interface[i].ip) {

			packet_ethernet_t* eth = (packet_ethernet_t *) pi->packet;
			addr_mac_t temp_mac = eth->dest_mac;
			eth->dest_mac = eth->source_mac;
			eth->source_mac = temp_mac;

			addr_ip_t temp_ip = ipv4->dst_ip;
			ipv4->dst_ip = ipv4->src_ip;
			ipv4->src_ip = temp_ip;

			ipv4->ttl = ipv4->ttl - 1;
			ipv4->flags_fragmentoffset = 0;
			ipv4->id = htons(id); // Fix
			id++;

			packet_icmp_t* icmp =
					(packet_icmp_t *) &pi->packet[sizeof(packet_ethernet_t)
							+ sizeof(packet_ip4_t)];
			icmp->type = ICMP_TYPE_REPLAY;

			icmp->header_checksum = 0;
			icmp->header_checksum = generatechecksum((unsigned short*) icmp,
					sizeof(packet_icmp_t));

			ipv4->header_checksum = 0;
			ipv4->header_checksum = generatechecksum((unsigned short*) ipv4,
					sizeof(packet_ip4_t));

			ethernet_packet_send(get_sr(), pi->interface, eth->dest_mac,
					eth->source_mac, htons(ETH_IP_TYPE), pi);

			return;

		}
	}

	ip_table_entry_t dest_ip_entry; // memory allocation ;(
	int id = ip_longestprefixmatch(&pi->router->ip_table, ipv4->dst_ip,
			&dest_ip_entry);

	if (id >= 0) {

		arp_cache_entry_t arp_dest; // memory allocation ;(
		int id = arp_getcachebyip(&pi->router->arp_cache, ipv4->dst_ip,
				&arp_dest);

		if (id >= 0) {
			ipv4->ttl--; // reduce ttl

			ipv4->header_checksum = 0;
			ipv4->header_checksum = generatechecksum((unsigned short*) ipv4,
					sizeof(packet_ip4_t));
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
	}

//	printf("\nIPv4 packet:\n");
//	printf("Version: 0x%x \n", ipv4->version_ihl & 0xF);
//	printf("IHL: 0x%x \n", (ipv4->version_ihl >> 4) & 0xF);
//	printf("DSCP ECN: 0x%x \n", ipv4->dscp_ecn);
//	printf("Total length: %d \n", ntohs(ipv4->total_length));
//	printf("Id: %d \n", ntohs(ipv4->id));
//	printf("Flags fragment offset: 0x%x \n", ntohs(ipv4->flags_fragmentoffset));
//	printf("TTL: %d \n", ipv4->ttl);
//	printf("Protocol: %d (0x%x) \n", ipv4->protocol, ipv4->protocol);
//	printf("Headerchecksum: %d (0x%x)\n", ntohs(ipv4->header_checksum), ntohs(ipv4->header_checksum));
//	printf("Source ip: %s\n", quick_ip_to_string(ipv4->src_ip));
//	printf("Destination ip: %s\n", quick_ip_to_string(ipv4->dst_ip));
//	if (dest_ip_entry!= NULL) printf("Longest prefix match on %s interface\n", dest_ip_entry->interface->name);

}

