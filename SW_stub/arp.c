#include "arp.h"
#include <stdio.h>
#include <netinet/in.h>

#include "sr_interface.h"
#include "sr_integration.h"
#include "ethernet_packet.h"

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>

#define ARP_OP_REQUEST (1)
#define ARP_OP_REPLY (2)

#define MAC_TO_UINT64(mac) ((uint64_t) (*((uint64_t *)(&mac))))
#define IP_0(ip) (ip & 0xFF)
#define IP_1(ip) ((ip >> 8) & 0xFF)
#define IP_2(ip) ((ip >> 16) & 0xFF)
#define IP_3(ip) ((ip >> 24) & 0xFF)
#define IP_PRINT(ip) IP_0(ip), IP_1(ip), IP_2(ip), IP_3(ip)

void arp_print_cache(arp_cache_t * cache) {
	int i;
	printf("THE ARP TABLE\n-------------\n\n");
	for (i = 0; i < cache->cache_size; i++)
		printf("%d. MAC: %lX, IP: %d.%d.%d.%d, iface %s\n",
				i, MAC_TO_UINT64(cache->entries[i].mac),
				IP_PRINT(cache->entries[i].ip),
				cache->entries[i].interface->name);
	printf("\n");
}

arp_cache_entry_t * arp_getcachebymac(arp_cache_t * cache, addr_mac_t mac) {
	int i;
	for (i = 0; i < cache->cache_size; i++)
		if (cache->entries[i].mac.octet[0] == mac.octet[0] && cache->entries[i].mac.octet[1] == mac.octet[1] &&
				cache->entries[i].mac.octet[2] == mac.octet[2] && cache->entries[i].mac.octet[3] == mac.octet[3] &&
				cache->entries[i].mac.octet[4] == mac.octet[4] && cache->entries[i].mac.octet[5] == mac.octet[5])
			return &cache->entries[i];
	return NULL;
}

arp_cache_entry_t * arp_getcachebyip(arp_cache_t * cache, addr_ip_t ip) {
	int i;
	for (i = 0; i < cache->cache_size; i++)
		if (cache->entries[i].ip == ip)
			return &cache->entries[i];
	return NULL;
}

void arp_putincache(arp_cache_t * cache, addr_ip_t ip, addr_mac_t mac, interface_t* interface) {
	arp_cache_entry_t * existing = arp_getcachebymac(cache, mac);
	if (existing != NULL) {
		// just the guy with this mac got a new ip
		existing->ip = ip;
		existing->interface = interface;
		return;
	}

	existing = arp_getcachebyip(cache, ip);
	if (existing != NULL) {
		// just the guy with this mac got a new ip
		existing->mac = mac;
		existing->interface = interface;
		return;
	}

	const int id = cache->cache_size;

	if (cache->cache_size == 0) {
		cache->cache_size++;
		cache->entries = (arp_cache_entry_t *) malloc(sizeof(arp_cache_entry_t) * cache->cache_size);
	} else {
		cache->cache_size++;
		cache->entries = (arp_cache_entry_t *) realloc((void *) cache->entries, sizeof(arp_cache_entry_t) * cache->cache_size);
	}

	cache->entries[id].interface = interface;
	cache->entries[id].ip = ip;
	cache->entries[id].mac = mac;

	arp_print_cache(cache);
}

void arp_send(struct sr_instance* sr, interface_t* intf, uint8_t opcode, addr_ip_t target_ip, addr_mac_t target_mac, addr_ip_t source_ip, addr_mac_t source_mac) {

	packet_arp_t arpdiscovery = {
			.hardwareaddresslength = 6,
			.protocoladdresslength = 4,
			.opcode = htons(opcode),
			.protocoltype = htons(ARP_PTYPE_IP),
			.hardwaretype = htons(ARP_HTYPE_ETH),
			.target_ip = target_ip,
			.target_mac = target_mac,
			.sender_ip = source_ip,
			.sender_mac = source_mac
	};

	ethernet_packet_send(sr, intf, target_mac, source_mac, htons(ETH_ARP_TYPE), (byte *) &arpdiscovery, sizeof(arpdiscovery));
}

void arp_onreceive(packet_info_t* pi, packet_arp_t * arp, byte * payload, int payload_len) {
	// decision logic
	const int hardwaretype = ntohs(arp->hardwaretype);
	const int protocoltype = ntohs(arp->protocoltype);

	if (hardwaretype == ARP_HTYPE_ETH && protocoltype == ARP_PTYPE_IP && arp->hardwareaddresslength == 6 && arp->protocoladdresslength == 4) {
		arp_cache_t * cache = &pi->router->arp_cache;

		arp_putincache(cache, arp->sender_ip, arp->sender_mac, pi->interface);

		const int opcode = ntohs(arp->opcode);
		switch(opcode) {
		case ARP_OPCODE_REQUEST:
		{
			// Who has arp->target_ip Tell arp->sender_ip
			arp_cache_entry_t * result = arp_getcachebyip(cache, arp->target_ip);
			if (result == NULL)
				fprintf(stderr, "No one has %d.%d.%d.%d\n",IP_PRINT(arp->target_ip));
			else
				arp_send(get_sr(), pi->interface, ARP_OP_REPLY, arp->sender_ip, arp->sender_mac, result->ip, result->mac);
			break;
		}
		default:
			fprintf(stderr, "Unsupported ARP opcode %x!\n", opcode);
			break;
		}
	}
	else fprintf(stderr, "Unsupported ARP packet!\n");
}
