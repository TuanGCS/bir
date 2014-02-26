/* Filename: sr_router.c */

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include "common/nf10util.h"
#include "sr_cpu_extension_nf2.h"
#include "sr_router.h"

#include "packets.h"
#include "arp.h"
#include "ip.h"
#include "sr_interface.h"
#include "globals.h"

void router_init(router_t* router) {
#ifdef _CPUMODE_
	init_registers(router);
	router->nf.device_name = "nf10";
	check_iface( &router->nf );
	if( openDescriptor( &router->nf ) != 0 )
	die( "Error: failed to connect to the hardware" );
	else {
		/* wait for the reset to complete */
		struct timespec pause;
		pause.tv_sec = 0;
		pause.tv_nsec = 5000 * 1000; /* 5ms */
		nanosleep( &pause, NULL );
	}
#endif

	router->is_router_running = 1;
	router->num_interfaces = 0;

	router->use_ospf = TRUE;

	pthread_mutex_init(&router->intf_lock, NULL);

	queue_init(&router->arp_cache);
	queue_init(&router->ip_table);
	queue_init(&router->iparp_buffer);

#ifndef _THREAD_PER_PACKET_
	debug_println("Initializing the router work queue with %u worker threads",
	NUM_WORKER_THREADS);
	wq_init(&router->work_queue, NUM_WORKER_THREADS, &router_handle_work);
#else
	debug_println( "Router initialized (will use one thread per packet)" );
#endif
}

void router_destroy(router_t* router) {
	router->is_router_running = 0;

	pthread_mutex_destroy(&router->intf_lock);

	queue_free(&router->arp_cache);
	queue_free(&router->ip_table);
	queue_free(&router->iparp_buffer);

#ifdef _CPUMODE_
	closeDescriptor( &router->nf );
#endif

#ifndef _THREAD_PER_PACKET_
	wq_destroy(&router->work_queue);
#endif
}

void router_handle_packet(packet_info_t* pi) {

	if (PACKET_CAN_MARSHALL(packet_ethernet_t, 0, pi->len)) {

		packet_ethernet_t * eth_packet = PACKET_MARSHALL(packet_ethernet_t,
				pi->packet, 0);
		const int type = ntohs(eth_packet->type);

		switch (type) {
		case ETH_ARP_TYPE:
			if (PACKET_CAN_MARSHALL(packet_arp_t, sizeof(packet_ethernet_t),
					pi->len)) {
				packet_arp_t * arp_packet = PACKET_MARSHALL(packet_arp_t,
						pi->packet, sizeof(packet_ethernet_t));
				arp_onreceive(pi, arp_packet);
			} else
				fprintf(stderr, "Invalid ARP packet!\n");
			break;
		case ETH_IP_TYPE:
			if (PACKET_CAN_MARSHALL(packet_ip4_t, sizeof(packet_ethernet_t),
					pi->len)) {
				packet_ip4_t * ip_packet = PACKET_MARSHALL(packet_ip4_t,
						pi->packet, sizeof(packet_ethernet_t));
				ip_onreceive(pi, ip_packet);
			} else
				fprintf(stderr, "Invalid IP packet!\n");
			break;
		case ETH_RARP_TYPE:
			fprintf(stderr, "Received RARP which is currently unsupported\n");
			break;
		default:
			fprintf(stderr, "Unsupported protocol type %x \n",
					eth_packet->type);
			break;
		}
	} else {
		fprintf(stderr, "Invalid Ethernet packet! \n");
	}

}

#ifdef _THREAD_PER_PACKET_
void* router_pthread_main( void* vpacket ) {
	static unsigned id = 0;
	char name[15];
	snprintf( name, 15, "PHandler %u", id++ );
	debug_pthread_init( name, "Packet Handler Thread" );
	pthread_detach( pthread_self() );
	router_handle_packet( (packet_info_t*)vpacket );
	debug_println( "Packet Handler Thread is shutting down" );
	return NULL;
}
#else
void router_handle_work(work_t* work) {
	/* process the work */
	switch (work->type) {
	case WORK_NEW_PACKET:
		router_handle_packet((packet_info_t*) work->work);
		break;

	default:
		die("Error: unknown work type %u", work->type);
		break;
	}
}
#endif

interface_t* router_lookup_interface_via_ip(router_t* router, addr_ip_t ip) {
	ip_table_entry_t entry;
	if (ip_longestprefixmatch(&router->ip_table, ip, &entry) >= 0)
		return entry.interface;
	else
		return NULL;
}

interface_t* router_lookup_interface_via_name(router_t* router,
		const char* name) {
	unsigned i;
	return NULL;
}

void router_add_interface(router_t* router, const char* name, addr_ip_t ip,
		addr_ip_t mask, addr_mac_t mac) {

	if (router->num_interfaces == 0) {
		router->pw_router.router_id = ip;
		router->pw_router.area_id = ip & mask;
		router->pw_router.lsuint = LSUINT;
	}

	interface_t* intf;

	debug_println("called router_add_interface");  // TODO remove debugging line

	intf = &router->interface[router->num_interfaces];

	strcpy(intf->name, name);
	intf->ip = ip;
	intf->subnet_mask = mask;
	intf->mac = mac;
	intf->enabled = TRUE;
	intf->helloint = HELLOINT;
	queue_init(&intf->neighbours);

	arp_putincache(&router->arp_cache, ip, mac, ARP_CACHE_TIMEOUT_STATIC);

#ifdef MININET_MODE
	// open a socket to talk to the hw on this interface
	debug_println("*******iface %s (check the name!)\n", name);
	intf->hw_fd = sr_mininet_init_intf_socket_withname(name); // (router name isn't used in this version)

	// set pretty hw_id
	if (strcmp(name + PREFIX_LENGTH, "eth0") == 0)
		intf->hw_id = INTF0;
	else if (strcmp(name + PREFIX_LENGTH, "eth1") == 0)
		intf->hw_id = INTF1;
	else if (strcmp(name + PREFIX_LENGTH, "eth2") == 0)
		intf->hw_id = INTF2;
	else if (strcmp(name + PREFIX_LENGTH, "eth3") == 0)
		intf->hw_id = INTF3;
	else {
		debug_println(
				"Unknown interface name: %s. Setting hw_id to interface number.\n",
				name);
		intf->hw_id = router->num_interfaces;
	}

	// initialize the lock to ensure only one write per interface at a time
	pthread_mutex_init(&intf->hw_lock, NULL);
#endif

	router->num_interfaces += 1;

	// TODO! query the computers on those interfaces
}

int packetinfo_ip_allocate(router_t* router, packet_info_t ** pinfo, int size,
		addr_ip_t dest, int protocol) {
	(*pinfo) = (packet_info_t *) malloc(sizeof(packet_info_t));
	(*pinfo)->len = size;
	(*pinfo)->packet = (byte *) malloc(size);
	(*pinfo)->router = router;

	ip_table_entry_t entry;
	if (ip_longestprefixmatch(&router->ip_table, dest, &entry) >= 0) {
		(*pinfo)->interface = entry.interface;

		packet_ethernet_t* eth = (packet_ethernet_t *) (*pinfo)->packet;
		eth->source_mac = entry.interface->mac;
		arp_cache_entry_t arpentry;
		if (arp_getcachebyip(&router->arp_cache, dest, &arpentry) >= 0)
			eth->dest_mac = arpentry.mac;
		else {
			addr_mac_t boradcast = MAC_BROADCAST;
			eth->dest_mac = boradcast;
		}

		eth->type = htons(ETH_IP_TYPE);

		packet_ip4_t* ipv4 =
				(packet_ip4_t *) &(*pinfo)->packet[sizeof(packet_ethernet_t)];

		ipv4->ihl = 5;
		ipv4->version = 4;
		ipv4->dscp_ecn = 0;
		ipv4->total_length = htons(84);
		ipv4->id = 0;
		ipv4->flags_fragmentoffset = 0;
		ipv4->ttl = 64;
		ipv4->protocol = protocol;
		ipv4->header_checksum = 0;
		ipv4->src_ip = entry.interface->ip;
		ipv4->dst_ip = dest;

		ipv4->header_checksum = generatechecksum((unsigned short*) ipv4,
				sizeof(packet_ip4_t));

	} else {
		packetinfo_free(pinfo);
		return 0;
	}

	return 1;

}

void packetinfo_free(packet_info_t ** pinfo) {
	free((*pinfo)->packet);
	free((*pinfo));
	(*pinfo) = NULL;
}

