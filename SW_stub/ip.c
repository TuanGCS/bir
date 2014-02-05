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

volatile uint16_t hid = 1;

void ip_print_table(dataqueue_t * table) {

	int i;
	printf("THE IP TABLE\n-------------\n\n");
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

void ip_putintable(dataqueue_t * table, addr_ip_t ip, interface_t* interface,
		int netmask) {

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

	queue_add(table, (void *) &existing, sizeof(ip_table_entry_t));

	// for debug TODO remove after use
	ip_print_table(table);
}

int generatechecksum(unsigned short * buf, int len) {

	uint16_t * data = (uint16_t *) buf;
	int size = len / 2;

	uint32_t sum;
	for (sum = 0; size > 0; size--)
		sum += ntohs(*data++);
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return htons(((uint16_t) ~sum));
}

void sr_transport_input(uint8_t* packet /* borrowed */); // this function does the transport input to the system
void ip_onreceive(packet_info_t* pi, packet_ip4_t * ipv4) {

	// TODO! check ipv4 version, flags, header size, etc.

	if (generatechecksum((unsigned short*) ipv4, sizeof(packet_ip4_t)) != (0)) {
		fprintf(stderr, "IPv4 CHECKSUM ERR\n");
		return;
	}

	if (ipv4->ttl < 1)
		return; // don't handle packets with ttl less than 1 TODO should that be less than 0

	int i;
	for (i = 0; i < pi->router->num_interfaces; i++) {
		if (ipv4->dst_ip == pi->router->interface[i].ip) {

			// TODO! CHECK TYPE OF IP
			sr_transport_input((uint8_t *) ipv4);
			continue;

			packet_ethernet_t* eth = (packet_ethernet_t *) pi->packet;
			addr_mac_t temp_mac = eth->dest_mac;
			eth->dest_mac = eth->source_mac;
			eth->source_mac = temp_mac;

			addr_ip_t temp_ip = ipv4->dst_ip;
			ipv4->dst_ip = ipv4->src_ip;
			ipv4->src_ip = temp_ip;

			ipv4->ttl = 64; // Set back to max
			ipv4->flags_fragmentoffset = 0;
			ipv4->id = htons(hid); // Fix
			hid++;

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
			ipv4->ttl--;
			if(ipv4->ttl < 1)
				return;

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

}

