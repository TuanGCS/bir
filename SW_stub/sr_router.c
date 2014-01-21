/* Filename: sr_router.c */

#include <arpa/inet.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include "common/nf10util.h"
#include "sr_cpu_extension_nf2.h"
#include "sr_router.h"

#include "packets.h"


void router_init( router_t* router ) {
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

    router->num_interfaces = 0;

    router->use_ospf = TRUE;

    pthread_mutex_init( &router->intf_lock, NULL );

#ifndef _THREAD_PER_PACKET_
    debug_println( "Initializing the router work queue with %u worker threads",
                   NUM_WORKER_THREADS );
    wq_init( &router->work_queue, NUM_WORKER_THREADS, &router_handle_work );
#else
    debug_println( "Router initialized (will use one thread per packet)" );
#endif
}

void router_destroy( router_t* router ) {
    pthread_mutex_destroy( &router->intf_lock );

#ifdef _CPUMODE_
    closeDescriptor( &router->nf );
#endif

#ifndef _THREAD_PER_PACKET_
    wq_destroy( &router->work_queue );
#endif
}

void router_handle_arp(packet_ARP_t * arp, byte * payload, int payload_len) {
	// decision logic
	if (arp->hardwaretype == ARP_HTYPE_ETH_NO && arp->protocoltype == ARP_PTYPE_IP_NO && arp->hardwareaddresslength == 6 && arp->protocoladdresslength == 4) {
		const int opcode = ntohs(arp->opcode);
		switch(opcode) {
		case ARP_OPCODE_REQUEST:
			printf("ARP Request: Who has %d.%d.%d.%d? Tell %d.%d.%d.%d\n",
					arp->target_ip[0], arp->target_ip[1], arp->target_ip[2], arp->target_ip[3],
					arp->sender_ip[0], arp->sender_ip[1], arp->sender_ip[2], arp->sender_ip[3]);
			break;
		default:
			fprintf(stderr, "Unsupported ARP opcode %x!\n", opcode);
			break;
		}
	}
		else fprintf(stderr, "Unsupported ARP packet!\n");
}

void router_handle_packet( packet_info_t* pi ) {
	byte * payload = pi->packet;
	int len = pi->len;

	if (PACKET_CAN_MARSHALL(packet_Ethernet_t, len)) {
		packet_Ethernet_t * eth_packet = PACKET_MARSHALL(packet_Ethernet_t, payload, len);
		const int type = ntohs(eth_packet->type);
		switch(type) {
		case ETH_ARP_TYPE:
			if (PACKET_CAN_MARSHALL(packet_ARP_t, len)) {
				packet_ARP_t * arp_packet = PACKET_MARSHALL(packet_ARP_t, payload, len);
				router_handle_arp(arp_packet, payload, len);
			} else
				fprintf(stderr, "Invalid ARP packet!\n");
			break;
		default:
			fprintf(stderr, "Unknown ethernet type %x!\n", eth_packet->type);
			break;
		}
	} else
		fprintf(stderr, "Invalid Ethernet packet!\n");

//    packet_IPv4_header_t * ipv4 = (packet_IPv4_header_t *) packet;
//    printf("IPv4 packet:\n");
//    printf("Version ihl: 0x%x \n", ipv4->version_ihl);
//    printf("DSCP ECN: 0x%x \n", ipv4->dscp_ecn);
//    printf("Total length: %d \n", ipv4->total_length);
//    printf("Id: %d \n", ipv4->id);
//    printf("Flags fragment offset: 0x%x \n", ipv4->flags_fragmentoffset);
//    printf("TTL: %d \n", ipv4->ttl);
//    printf("Protocol: %d (0x%x) \n", ipv4->protocol, ipv4->protocol);
//    printf("Headerchecksum: %d (0x%x)\n", ipv4->header_checksum, ipv4->header_checksum);
//    printf("Source ip: %d.%d.%d.%d\n", ipv4->sourceip & 0xFF, (ipv4->sourceip >> 8) & 0xFF, (ipv4->sourceip >> 16) & 0xFF, (ipv4->sourceip >> 24) & 0xFF);
//    printf("Destination ip: %d.%d.%d.%d\n", ipv4->destionationip & 0xFF, (ipv4->destionationip >> 8) & 0xFF, (ipv4->destionationip >> 16) & 0xFF, (ipv4->destionationip >> 24) & 0xFF);
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
void router_handle_work( work_t* work ) {
    /* process the work */
    switch( work->type ) {
    case WORK_NEW_PACKET:
        router_handle_packet( (packet_info_t*)work->work );
        break;

    default:
        die( "Error: unknown work type %u", work->type );
    }
}
#endif


interface_t* router_lookup_interface_via_ip( router_t* router, addr_ip_t ip ) {
    return 0;
}

interface_t* router_lookup_interface_via_name( router_t* router,
                                               const char* name ) {
    unsigned i;
    return NULL;
}

void router_add_interface( router_t* router,
                           const char* name,
                           addr_ip_t ip, addr_ip_t mask, addr_mac_t mac ) {
    interface_t* intf;

    debug_println("called router_add_interface");    // TODO remove debugging line

    intf = &router->interface[router->num_interfaces];

    strcpy( intf->name, name );
    intf->ip = ip;
    intf->subnet_mask = mask;
    intf->mac = mac;
    intf->enabled = TRUE;
    intf->neighbor_list_head = NULL;

#ifdef MININET_MODE
    // open a socket to talk to the hw on this interface
    debug_println("*******iface %s (check the name!)\n", name);
    intf->hw_fd = sr_mininet_init_intf_socket_withname(name);   // (router name isn't used in this version)

    // set pretty hw_id 
    if(      strcmp(name+PREFIX_LENGTH,"eth0")==0 ) intf->hw_id = INTF0;
    else if( strcmp(name+PREFIX_LENGTH,"eth1")==0 ) intf->hw_id = INTF1;
    else if( strcmp(name+PREFIX_LENGTH,"eth2")==0 ) intf->hw_id = INTF2;
    else if( strcmp(name+PREFIX_LENGTH,"eth3")==0 ) intf->hw_id = INTF3;
    else {
      debug_println( "Unknown interface name: %s. Setting hw_id to interface number.\n", name );
      intf->hw_id = router->num_interfaces;
    }

    // initialize the lock to ensure only one write per interface at a time
    pthread_mutex_init( &intf->hw_lock, NULL );
#endif

    router->num_interfaces += 1;

}


