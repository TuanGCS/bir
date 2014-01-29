#include "ip.h"
#include <stdio.h>
#include <netinet/in.h>

#include "sr_interface.h"
#include "sr_integration.h"
#include "ethernet_packet.h"
#include "arp.h"

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>

void ip_print_table(ip_table_t * table) {
	int i;
	printf("THE IP TABLE\n-------------\n\n");
	for (i = 0; i < table->table_size; i++)
		printf("%d. IP: %s @ iface %s\n",
				i,
				quick_ip_to_string(table->entries[i].ip),
				table->entries[i].interface->name);
	printf("\n");
}

// TODO! thread safety
// TODO! what happens if two interfaces match?
ip_table_entry_t * ip_longestprefixmatch(ip_table_t * table, addr_ip_t ip) {
	int maxmatch = 0;
	ip_table_entry_t * answer = NULL;

	int i;
	for (i = 0; i < table->table_size; i++) {
		const uint32_t tableip = table->entries[i].ip;

		if (ip == tableip) {
			// we won't find a better match
			return &table->entries[i];
		} else if ((ip & 0xFFFFFF) == (tableip & 0xFFFFFF)) {
			if (maxmatch < 3) {
				maxmatch = 3;
				answer = &table->entries[i];
			}
		}
		else if ((ip & 0xFFFF) == (tableip & 0xFFFF)) {
			if (maxmatch < 2) {
				maxmatch = 2;
				answer = &table->entries[i];
			}
		}
		else if ((ip & 0xFF) == (tableip & 0xFF)) {
			if (maxmatch < 1) {
				maxmatch = 1;
				answer = &table->entries[i];
			}
		}
	}

	return answer;
}

// TODO! thread safety
ip_table_entry_t * ip_directmatch(ip_table_t * table, addr_ip_t ip) {
	int i;
	for (i = 0; i < table->table_size; i++)
		if (table->entries[i].ip == ip)
			return &table->entries[i];
	return NULL;
}

// TODO! thread safety
void ip_putintable(ip_table_t * table, addr_ip_t ip, interface_t* interface) {

	ip_table_entry_t * existing = ip_directmatch(table, ip);
	if (existing != NULL) {
		if(existing->interface == interface)
			return; // nothing to do, this already exists
		else
			fprintf(stderr, "Possible IP conflict! %s was already registered at %s but now there is another %s that wants to be registered on %s!\n",
					quick_ip_to_string(existing->ip), existing->interface->name,
					quick_ip_to_string(ip), interface->name);
	}

	const int id = table->table_size;

	if (table->table_size == 0) {
		table->table_size++;
		table->entries = (ip_table_entry_t *) malloc(sizeof(ip_table_entry_t) * table->table_size);
	} else {
		table->table_size++;
		table->entries = (ip_table_entry_t *) realloc((void *) table->entries, sizeof(ip_table_entry_t) * table->table_size);
	}

	table->entries[id].interface = interface;
	table->entries[id].ip = ip;

	// for debug TODO remove after use
	ip_print_table(table);
}

static inline int ip_checkchecksum(packet_ip4_t * ipv4) {
	uint16_t * data = (uint16_t *) ipv4;
	int size = sizeof(packet_ip4_t)/2;

	uint32_t sum;
	for (sum = 0; size > 0; size--)
	   sum += ntohs(*data++);
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return ((uint16_t)~sum) == (0);
}

static inline void ip_generatechecksum(packet_ip4_t * ipv4) {
	ipv4->header_checksum = 0;

	uint16_t * data = (uint16_t *) ipv4;
	int size = sizeof(packet_ip4_t)/2;

	uint32_t sum;
	for (sum = 0; size > 0; size--)
	   sum += ntohs(*data++);
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	ipv4->header_checksum = htons(((uint16_t)~sum));
}


void ip_onreceive(packet_info_t* pi, packet_ip4_t * ipv4) {

	// TODO! check ipv4 version, flags, header size, etc.

	if (ipv4->ttl < 1) return; // don't handle packets with ttl less than 1 TODO should that be less than 0

	ip_table_entry_t * dest_ip_entry = ip_longestprefixmatch(&pi->router->ip_table, ipv4->dst_ip);

	if (!ip_checkchecksum(ipv4)) {
		fprintf(stderr,"IPv4 CHECKSUM ERR\n");
		return;
	}

	if (dest_ip_entry != NULL) {
		ipv4->ttl--; // reduce ttl

		arp_cache_entry_t * arp_dest = arp_getcachebyip(&pi->router->arp_cache, ipv4->dst_ip);

		if (arp_dest != NULL) {
			ip_generatechecksum(ipv4); // generate checksum
			ethernet_packet_send(get_sr(), dest_ip_entry->interface, arp_dest->mac, dest_ip_entry->interface->mac, htons(ETH_IP_TYPE), pi);
		} else {
			arp_send_request(pi->router, dest_ip_entry->interface, ipv4->dst_ip);
			fprintf(stderr, "Cannot forward IP packet! No entry in ARP table\n");
		}

	} else
		printf("Longest prefix matching failed to find an interface for %s\n", quick_ip_to_string(ipv4->dst_ip));

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

