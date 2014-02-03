#include "arp.h"
#include <stdio.h>
#include <netinet/in.h>

#include "sr_interface.h"
#include "sr_integration.h"
#include "ethernet_packet.h"

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>
#include "packets.h"

#define ARP_OP_REQUEST (1)
#define ARP_OP_REPLY (2)

void arp_print_cache(dataqueue_t * cache) {
	int i;
	printf("THE ARP TABLE\n-------------\n\n");
	for (i = 0; i < cache->size; i++) {
		arp_cache_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(arp_cache_entry_t));

			printf("%d. MAC: %s, IP: %s, iface %s, timeout %ld \n", i,
					quick_mac_to_string(&entry->mac),
					quick_ip_to_string(entry->ip),
					entry->interface->name,
					entry->tv.tv_sec);

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

int arp_getcachebymac(dataqueue_t * cache, addr_mac_t mac, arp_cache_entry_t * result) {
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

int arp_getcachebyip(dataqueue_t * cache, addr_ip_t ip, arp_cache_entry_t * result) {
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

void arp_putincache(dataqueue_t * cache, addr_ip_t ip, addr_mac_t mac,
		interface_t* interface, uint8_t timeout) {

	arp_cache_entry_t result;

	int id = arp_getcachebymac(cache, mac, &result);
	if (id  >= 0) {
		// just the guy with this mac got a new ip
		result.ip = ip;
		result.interface = interface;

		gettimeofday(&result.tv, NULL);
		if (timeout != -1) {
			timeout += result.tv.tv_sec;
		}
		result.tv.tv_sec = timeout;

		queue_replace(cache, &result, sizeof(arp_cache_entry_t), id);
		return;
	}

	id = arp_getcachebyip(cache, ip, &result);
	if (id  >= 0) {
		// just the guy with this mac got a new ip
		result.mac = mac;
		result.interface = interface;

		gettimeofday(&result.tv, NULL);
		if (timeout != -1) {
			timeout += result.tv.tv_sec;
		}
		result.tv.tv_sec = timeout;

		queue_replace(cache, &result, sizeof(arp_cache_entry_t), id);

		return;
	}

	result.interface = interface;
	result.ip = ip;
	result.mac = mac;

	gettimeofday(&result.tv, NULL);
	if (timeout != -1) {
		timeout += result.tv.tv_sec;
	}
	result.tv.tv_sec = timeout;

	queue_add(cache, &result, sizeof(arp_cache_entry_t));

	arp_print_cache(cache);
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
		dataqueue_t * cache = &pi->router->arp_cache;

		const int opcode = ntohs(arp->opcode);
		switch (opcode) {

		case ARP_OPCODE_REQUEST: {

			// ARPs to remote subnetworks are send to the gateway address
			if (arp->target_ip == pi->interface->ip) {
				arp_putincache(cache, arp->sender_ip, arp->sender_mac,
						pi->interface, ARP_CACHE_TIMEOUT_REQUEST);

				arp_send(get_sr(), pi->interface, arp, pi, ARP_OP_REPLY,
						arp->sender_ip, arp->sender_mac, pi->interface->ip,
						pi->interface->mac);
			} else {
				arp_putincache(cache, arp->sender_ip, arp->sender_mac,
						pi->interface, ARP_CACHE_TIMEOUT_BROADCAST);
			}

			break;
		}

		case ARP_OPCODE_REPLAY: {
			if (arp->target_ip == pi->interface->ip
					&& match_mac(arp->target_mac, pi->interface->mac)) {
				arp_putincache(cache, arp->sender_ip, arp->sender_mac,
						pi->interface, ARP_CACHE_TIMEOUT_REQUEST);
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
void arp_maintain_cache(dataqueue_t * cache) {

	int i;
	struct timespec timeout, ts;
	struct timeval tv_diff;
	timeout.tv_sec = 0;
	timeout.tv_nsec = 5000000; // 0.5s

	while (1) {

		nanosleep(&timeout, &ts);

		for (i = 0; i < cache->size; i++) {
			arp_cache_entry_t * entry;
			int entry_size;
			if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

				assert(entry_size == sizeof(arp_cache_entry_t));

				gettimeofday(&tv_diff, NULL);

				// Check if dynamic entry and if expired
				if (entry->tv.tv_sec != -1
						&& difftime(entry->tv.tv_sec, tv_diff.tv_sec)
							<= 0) {
					// REMOVE
					queue_unlockidandremove(cache, i);
				} else
					queue_unlockid(cache, i);
			}
		}

		if (cache->size > ARP_THRESHOLD) {
			// CLEAN
		}

	}

}

// Add a static entry given IP, MAC and Interface
void arp_add_static(packet_info_t* pi, addr_ip_t ip, addr_mac_t mac,
		interface_t* interface) {

	arp_putincache(&pi->router->arp_cache, ip, mac, interface, ARP_CACHE_TIMEOUT_STATIC);

}

// Remove static entries based on IP address
void arp_remove_static_ip(packet_info_t* pi, addr_ip_t ip) {
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

}

// Remove static entries based on MAC address
void arp_remove_static_mac(packet_info_t* pi, addr_mac_t mac) {

	dataqueue_t * cache = &pi->router->arp_cache;

	int i;
	for (i = 0; i < cache->size; i++) {
		arp_cache_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(arp_cache_entry_t));

			if (match_mac(entry->mac, mac)
							&& entry->tv.tv_sec == -1) {
				// REMOVE
				queue_unlockidandremove(cache, i);
			} else
				queue_unlockid(cache, i);
		}
	}

}

// Delete all static entries in the ARP cache
void arp_clear_static(packet_info_t* pi) {
	dataqueue_t * cache = &pi->router->arp_cache;

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
}

void arp_send_request(router_t* router, interface_t* interface, addr_ip_t target_ip) {

	packet_info_t pi;
	pi.router = router;
	pi.packet = (byte *) malloc(
			sizeof(packet_ethernet_t) + sizeof(packet_arp_t));;
	pi.len = sizeof(packet_ethernet_t) + sizeof(packet_arp_t);
	pi.interface = interface;

	packet_arp_t* arp = (packet_arp_t *) &pi.packet[sizeof(packet_ethernet_t)];
	addr_mac_t mac_broadcast = {.octet = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}};

	arp_send(get_sr(), pi.interface, arp, &pi, ARP_OPCODE_REQUEST, target_ip,
			mac_broadcast, pi.interface->ip, pi.interface->mac);

	free(pi.packet);

}
