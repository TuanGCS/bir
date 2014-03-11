#include "arp.h"
#include <stdio.h>
#include <netinet/in.h>

#include "sr_interface.h"
#include "sr_integration.h"
#include "sr_common.h"
#include "ethernet_packet.h"

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>
#include <string.h>
#include "packets.h"
#include "ip.h"
#include <unistd.h>

#ifdef _CPUMODE_
#include "common/nf10util.h"
#include "reg_defines.h"
#endif


void arp_print_cache(dataqueue_t * cache) {
	int i;
	printf("\nTHE ARP TABLE\n-------------\n\n");
	for (i = 0; i < cache->size; i++) {
		arp_cache_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(arp_cache_entry_t));

			printf("%d. MAC: %s, IP: %s, timeout %ld \n", i,
					quick_mac_to_string(&entry->mac),
					quick_ip_to_string(entry->ip), entry->tv.tv_sec);

			queue_unlockid(cache, i);
		}
	}
	printf("\n");
}

int match_mac(addr_mac_t mac1, addr_mac_t mac2) {
	if (mac1.octet[0] == mac2.octet[0] && mac1.octet[1] == mac2.octet[1]
			&& mac1.octet[2] == mac2.octet[2] && mac1.octet[3] == mac2.octet[3]
			&& mac1.octet[4] == mac2.octet[4] && mac1.octet[5] == mac2.octet[5])
		return 1;
	else
		return 0;
}

int arp_getcachebymac(dataqueue_t * cache, addr_mac_t mac,
		arp_cache_entry_t * result) {
	int i;
	for (i = 0; i < cache->size; i++) {
		arp_cache_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(arp_cache_entry_t));

			if (match_mac(entry->mac, mac)) {
				*result = *entry; // do a memory copy so even after the function returns if this entry is removed, we won't get an error
				queue_unlockid(cache, i);
				return i;
			}

			queue_unlockid(cache, i);
		}
	}
	return -1;
}

int arp_getcachebyip(dataqueue_t * cache, addr_ip_t ip,
		arp_cache_entry_t * result) {
	int i;
	for (i = 0; i < cache->size; i++) {
		arp_cache_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(arp_cache_entry_t));

			if (entry->ip == ip) {
				*result = *entry; // do a memory copy so even after the function returns if this entry is removed, we won't get an error
				queue_unlockid(cache, i);
				return i;
			}

			queue_unlockid(cache, i);
		}
	}
	return -1;
}

// looks for queued ip packets that are there due to unknown mac match to the ip and forward them
void process_arpipqueue(dataqueue_t * queue, addr_ip_t ip, addr_mac_t mac, router_t * router) {
	int i;
	for (i = 0; i < queue->size; i++) {
		byte * data;
		int data_size;
		if (queue_getidandlock(queue, i, (void **) &data, &data_size)) {

			assert(data_size > sizeof(packet_info_t));

			// construct a packet info from memory
			packet_info_t * entry = (packet_info_t *) data;
			entry->packet = &data[sizeof(packet_info_t)];

			assert(data_size == (entry->len + sizeof(packet_info_t)));

			if (PACKET_CAN_MARSHALL(packet_ip4_t, sizeof(packet_ethernet_t),
					entry->len)) {
				packet_ip4_t * ip_packet = PACKET_MARSHALL(packet_ip4_t,
						entry->packet, sizeof(packet_ethernet_t));

				rtable_entry_t dest_ip_entry; // memory allocation ;(

				if (ip_longestprefixmatch(&router->ip_table, ip_packet->dst_ip,
						&dest_ip_entry) >= 0) {

					if (dest_ip_entry.router_ip == 0) {
						dest_ip_entry.router_ip = ip_packet->dst_ip;
					}

				} else {
					printf("TODO");
				}

				if (ip == dest_ip_entry.router_ip) {
					// make a copy of the packet
					byte data_copy[data_size];
					memcpy(data_copy, data, data_size);
					packet_info_t * entry_copy = (packet_info_t *) data_copy;
					entry_copy->packet = &data_copy[sizeof(packet_info_t)];
					packet_ip4_t * ip_packet_copy = PACKET_MARSHALL(
							packet_ip4_t, entry_copy->packet,
							sizeof(packet_ethernet_t));

					queue_unlockidandremove(queue, i); // release queue

					// undo the ttl substraction orignally done by ip_headercheck
					ip_packet_copy->ttl++;

					// we have a match deal with the packet as if it was just received
					ip_onreceive(entry_copy, ip_packet_copy);

				} else
					queue_unlockid(queue, i);

			} else {
				queue_unlockid(queue, i);
				fprintf(stderr, "Invalid IP packet in queue!\n");
			}

		}
	}
}

#ifdef _CPUMODE_
void hardware_arp_putincache(router_t * router, addr_ip_t ip, addr_mac_t mac, int id) {
	if (id < 0 || id > 31) {
		fprintf(stderr, "ARP table is bigger than 32 addresses! This is not supported by hardware!\n");
		return;
	}

	ip = ntohl(ip);

	const uint32_t mac_low = mac_lo(&mac);
	const uint32_t mac_high = mac_hi(&mac);

	writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_IP, ip);
	writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_MAC_HIGH, mac_high);
	writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_MAC_LOW, mac_low);

	writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_WR_ADDR, id);


	writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_IP, 0);
	writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_MAC_HIGH, 0);
	writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_MAC_LOW, 0);

	writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_RD_ADDR, id);

	uint32_t read_ip, read_mac_low, read_mac_high;
	readReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_IP, &read_ip);
	readReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_MAC_HIGH, &read_mac_high);
	readReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_MAC_LOW, &read_mac_low);

	assert (read_ip == ip);
	assert (mac_low == read_mac_low);
	assert (mac_high == read_mac_high);
}
#endif
void hardware_arp_synccache(router_t * router) {
#ifdef _CPUMODE_
	dataqueue_t * cache = &router->arp_cache;
	int i;
	for (i = 0; i < 32; i++) {
		arp_cache_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(arp_cache_entry_t));

			hardware_arp_putincache(router, entry->ip, entry->mac, i);

			queue_unlockid(cache, i);
		} else {
			addr_mac_t maczero = MAC_ZERO;
			hardware_arp_putincache(router, 0, maczero, i);
		}
	}
#endif
}




void arp_putincache(router_t* router, dataqueue_t * cache, addr_ip_t ip, addr_mac_t mac, time_t timeout) {

	arp_cache_entry_t result;

	int id = arp_getcachebymac(cache, mac, &result);
	if (id >= 0) {
		// just the guy with this mac got a new ip
		result.ip = ip;

		gettimeofday(&result.tv, NULL);
		if (timeout != -1) {
			timeout += result.tv.tv_sec;
		}
		result.tv.tv_sec = timeout;

		queue_replace(cache, &result, sizeof(arp_cache_entry_t), id);
#ifdef _CPUMODE_
		hardware_arp_putincache(router, ip, mac, id);
#endif
		return;
	}

	id = arp_getcachebyip(cache, ip, &result);
	if (id >= 0) {
		// just the guy with this mac got a new ip
		result.mac = mac;

		gettimeofday(&result.tv, NULL);
		if (timeout != -1) {
			timeout += result.tv.tv_sec;
		}
		result.tv.tv_sec = timeout;

		queue_replace(cache, &result, sizeof(arp_cache_entry_t), id);
#ifdef _CPUMODE_
		hardware_arp_putincache(router, ip, mac, id);
#endif
		return;
	}

	result.ip = ip;
	result.mac = mac;

	gettimeofday(&result.tv, NULL);
	if (timeout != -1) {
		timeout += result.tv.tv_sec;
	}
	result.tv.tv_sec = timeout;

	queue_add(cache, &result, sizeof(arp_cache_entry_t));
#ifdef _CPUMODE_
	hardware_arp_putincache(router, ip, mac, queue_getcurrentsize(cache));
#endif
}

/* NOTE! After using arp_send, the original packet will be destroyed! Don't try to access fields in arp after the call of this function! */
void arp_send(struct sr_instance* sr, interface_t* intf, packet_arp_t * arp,
		packet_info_t* pi, uint8_t opcode, addr_ip_t target_ip,
		addr_mac_t target_mac, addr_ip_t source_ip, addr_mac_t source_mac) {

	arp->hardwareaddresslength = 6;
	arp->protocoladdresslength = 4;
	arp->opcode = htons(opcode);
	arp->protocoltype = htons(ARP_PTYPE_IP);
	arp->hardwaretype = htons(ARP_HTYPE_ETH);
	arp->target_ip = target_ip;
	arp->target_mac = target_mac;
	arp->sender_ip = source_ip;
	arp->sender_mac = source_mac;

	// This call does not send arp directly, it sends packet_start which is assumed to include arp
	// packet_start needs to point at the original ethernet packet that encapsulated arp
	ethernet_packet_send(sr, intf, target_mac, source_mac, htons(ETH_ARP_TYPE),
			pi);
}

void arp_onreceive(packet_info_t* pi, packet_arp_t * arp) {
	// decision logic
	const int hardwaretype = ntohs(arp->hardwaretype);
	const int protocoltype = ntohs(arp->protocoltype);

	if (hardwaretype == ARP_HTYPE_ETH && protocoltype == ARP_PTYPE_IP
			&& arp->hardwareaddresslength == 6
			&& arp->protocoladdresslength == 4) {

		// check for pending ip packets and process them
		process_arpipqueue(&pi->router->iparp_buffer, arp->sender_ip,
				arp->sender_mac, pi->router);

		dataqueue_t * cache = &pi->router->arp_cache;

		const int opcode = ntohs(arp->opcode);
		switch (opcode) {

		case ARP_OPCODE_REQUEST: {

			// ARPs to remote subnetworks are send to the gateway address
			if (arp->target_ip == pi->interface->ip) {
				arp_putincache(pi->router, cache, arp->sender_ip, arp->sender_mac,
				ARP_CACHE_TIMEOUT_REQUEST);

				arp_send(get_sr(), pi->interface, arp, pi, ARP_OP_REPLY,
						arp->sender_ip, arp->sender_mac, pi->interface->ip,
						pi->interface->mac);
			} else {
				arp_putincache(pi->router, cache, arp->sender_ip, arp->sender_mac,
				ARP_CACHE_TIMEOUT_BROADCAST);
			}

			break;
		}

		case ARP_OPCODE_REPLAY: {
			if (arp->target_ip == pi->interface->ip
					&& match_mac(arp->target_mac, pi->interface->mac)) {
				arp_putincache(pi->router, cache, arp->sender_ip, arp->sender_mac,
				ARP_CACHE_TIMEOUT_REQUEST);
				// Push packets from queue?
			} else {
				fprintf(stderr, "Invalid ARP Replay from %s!\n",
						quick_ip_to_string(arp->sender_ip));
			}
			break;
		}

		default:
			fprintf(stderr, "Unsupported ARP opcode %x!\n", opcode);
			break;
		}

	} else {
		fprintf(stderr, "Unsupported ARP packet!\n");
	}
}

// probably would require to change the argument to a router instance because of threading
void arp_maintain_cache(void *arg) {
	router_t * router = (router_t *) arg;
	pthread_detach(pthread_self());
	dataqueue_t * cache = &router->arp_cache;

	int i;
	struct timeval tv_diff;

	while (1) {

		sleep(5);

		int changed = 0;

		for (i = 0; i < cache->size; i++) {
			arp_cache_entry_t * entry;
			int entry_size;
			if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

				assert(entry_size == sizeof(arp_cache_entry_t));

				gettimeofday(&tv_diff, NULL);

				// Check if dynamic entry and if expired
				if (entry->tv.tv_sec != -1
						&& difftime(entry->tv.tv_sec, tv_diff.tv_sec) <= 0) {
					printf("Removing ARP entry %s ", quick_ip_to_string(entry->ip));
					printf("%s due to timeout\n", quick_mac_to_string(&entry->mac));
					changed = 1;
					queue_unlockidandremove(cache, i);
				} else
					queue_unlockid(cache, i);
			}
		}

		if (cache->size > ARP_THRESHOLD) {
			for (i = 0; i < ARP_THRESHOLD - cache->size; i++) {
				arp_cache_entry_t * entry;
				int entry_size;
				if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

					assert(entry_size == sizeof(arp_cache_entry_t));

					gettimeofday(&tv_diff, NULL);

					printf("Removing ARP entry %s ", quick_ip_to_string(entry->ip));
					printf("%s due to having too many entries in the ARP cache\n", quick_mac_to_string(&entry->mac));
					changed = 1;
					queue_unlockidandremove(cache, i);
				}
			}

		}

		if (changed)
			hardware_arp_synccache(router);

	}

}

// Add a static entry given IP, MAC and Interface
void arp_add_static(router_t * router, dataqueue_t * cache, addr_ip_t ip, addr_mac_t mac) {

	arp_putincache(router, cache, ip, mac, ARP_CACHE_TIMEOUT_STATIC);

}

void arp_remove_ip_mac(router_t * router, dataqueue_t * cache, addr_ip_t ip, addr_mac_t mac) {

	int i;
	for (i = 0; i < cache->size; i++) {
		arp_cache_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(arp_cache_entry_t));

			if (entry->ip == ip && match_mac(entry->mac, mac)) {
				// REMOVE
				queue_unlockidandremove(cache, i);
				return;
			} else
				queue_unlockid(cache, i);
		}
	}

	hardware_arp_synccache(router);
}

// Remove static entries based on IP address
void arp_remove_static_ip(router_t * router, packet_info_t* pi, addr_ip_t ip) {
	dataqueue_t * cache = &pi->router->arp_cache;

	int i;
	for (i = 0; i < cache->size; i++) {
		arp_cache_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(arp_cache_entry_t));

			if (entry->ip == ip && entry->tv.tv_sec == -1) {
				// REMOVE
				queue_unlockidandremove(cache, i);
			} else
				queue_unlockid(cache, i);
		}
	}

	hardware_arp_synccache(router);
}

// Delete all static entries in the ARP cache
void arp_clear_static(router_t * router, dataqueue_t * cache) {
	int i;
	for (i = 0; i < cache->size; i++) {
		arp_cache_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(arp_cache_entry_t));

			if (entry->tv.tv_sec == -1) {
				// REMOVE
				queue_unlockidandremove(cache, i);
			} else
				queue_unlockid(cache, i);
		}
	}

	hardware_arp_synccache(router);
}

void arp_clear_dynamic(router_t * router, dataqueue_t * cache) {
	int i;
	for (i = 0; i < cache->size; i++) {
		arp_cache_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(arp_cache_entry_t));

			if (entry->tv.tv_sec != -1) {
				// REMOVE
				queue_unlockidandremove(cache, i);
			} else
				queue_unlockid(cache, i);
		}
	}

	hardware_arp_synccache(router);
}

void arp_clear_all(router_t * router, dataqueue_t * cache) {
	queue_purge(cache);

	hardware_arp_synccache(router);
}

void arp_send_request(router_t* router, interface_t* interface,
		addr_ip_t target_ip) {

	packet_info_t pi;
	pi.router = router;
	pi.packet = (byte *) malloc(
			sizeof(packet_ethernet_t) + sizeof(packet_arp_t));
	;
	pi.len = sizeof(packet_ethernet_t) + sizeof(packet_arp_t);
	pi.interface = interface;

	packet_arp_t* arp = (packet_arp_t *) &pi.packet[sizeof(packet_ethernet_t)];
	addr_mac_t mac_broadcast =
			{ .octet = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF } };

	arp_send(get_sr(), pi.interface, arp, &pi, ARP_OPCODE_REQUEST, target_ip,
			mac_broadcast, pi.interface->ip, pi.interface->mac);

	free(pi.packet);

}
