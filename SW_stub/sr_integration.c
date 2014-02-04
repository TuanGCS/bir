
/*-----------------------------------------------------------------------------
 * Filename: sr_integration.c
 * Purpose: Methods called by the lowest-level of the network system to talk
 * with the network subsystem.  This is the entry point of integration for the
 * network layer.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "sr_base_internal.h"

#ifdef _CPUMODE_
#include "sr_cpu_extension_nf2.h"
#endif

#include "sr_common.h"
#include "sr_integration.h"
#include "sr_interface.h"
#include "sr_router.h"
#include "sr_thread.h"
#include "sr_work_queue.h"
#include "sr_dumper.h"

#include "ip.h"

/**
 * First method called during router initialization.
 * Reading in hardware information etc.
 */
void sr_integ_init(struct sr_instance* sr) {
    debug_println( "Initializing the router subsystem" );

    router_t* subsystem = malloc_or_die( sizeof(router_t) );

    router_init( subsystem );

#ifdef MININET_MODE
    subsystem->name = sr->router_name;      // router name (e.g. r0), needed for
#endif                                      // interface initialisation
    sr_set_subsystem( sr_get_global_instance(0), subsystem );
}

/**
 * Called after all initial hardware information (interfaces) have been
 * received.  Can be used to start subprocesses (such as dynamic-routing
 * protocol) which require interface information during initialization.
 */
void sr_integ_hw_setup( struct sr_instance* sr ) {
	router_t * router = sr_get_subsystem(sr);

    debug_println( "Performing post-hw setup initialization" );

	// TODO! STATIC IP TABLE REMOVE THIS
    ip_putintable(&router->ip_table, IP_CONVERT(10,0,1,2), &router->interface[0]);
    ip_putintable(&router->ip_table, IP_CONVERT(10,0,2,2), &router->interface[1]);
    ip_putintable(&router->ip_table, IP_CONVERT(10,0,3,2), &router->interface[2]);
}

/**
 * This method is called each time the router receives a packet on the
 * interface.  The packet buffer, the packet length and the receiving
 * interface are passed in as parameters. The packet is complete with
 * ethernet headers.
 *
 * Note: Make a copy of the
 * packet instead if you intend to keep it around beyond the scope of
 * the method call.
 */
void sr_integ_input(struct sr_instance* sr,
                    const uint8_t * packet/* borrowed */,
                    unsigned int len,
#if defined _CPUMODE_ || defined MININET_MODE
                    interface_t* intf )
#else
                    const char* interface )
#endif
{
    packet_info_t* pi;

    /* create a copy of the packet */
    pi = malloc_or_die( sizeof(*pi) ); /* freed by router_pthread_main */

    /* include info about the handling router and the packet's length */
    pi->router = sr->interface_subsystem;
    pi->len = len;

#if defined _CPUMODE_ || defined MININET_MODE
    pi->interface = intf;
#endif

    /* copy the (Ethernet) packet itself */
    pi->packet = malloc_or_die( len ); /* freed by router_pthread_main */
    memcpy( pi->packet, packet, len );

#ifdef _THREAD_PER_PACKET_
    pthread_t tid;
    /* handle the packet in a separate thread (it will detach itself) */
    make_thread( router_pthread_main, pi );
#else
    /* put the packet on the work queue */
    wq_enqueue( &pi->router->work_queue, WORK_NEW_PACKET, pi );
#endif
}


struct sr_instance* get_sr() {
    struct sr_instance* sr;

    sr = sr_get_global_instance( NULL );
    assert( sr );
    return sr;
}

router_t* get_router() {
    return get_sr()->interface_subsystem;
}


/**
 *
 * @return -1 on error and prints a message to stderr. Otherwise, 0 is returned.
 */
int sr_integ_low_level_output(struct sr_instance* sr /* borrowed */,
                              uint8_t* buf /* borrowed */ ,
                              unsigned int len,
                              interface_t* intf ) {
#ifdef _CPUMODE_
    return sr_cpu_output(buf /*lent*/, len, intf );
#else
# ifdef MININET_MODE
    return sr_mininet_output( buf /* lent*/, len, intf );
# else
#  ifdef _MANUAL_MODE_
    sr_log_packet(sr,buf,len);
    return sr_manual_send_packet( sr, buf /*lent*/, len, intf->name );
#  endif  /* _MANUAL_MODE_ */
# endif   /* MININET_MODE  */
#endif    /* _CPUMODE_     */
}

/** For memory deallocation pruposes on shutdown. */
void sr_integ_destroy(struct sr_instance* sr) {
    debug_println("Cleaning up the router for shutdown");
}

/**
 * Called by the transport layer for outgoing packets generated by the
 * router.  Expects source address in network byte order.
 *
 * @return 0 on failure to find a route to dest.
 */
uint32_t sr_integ_findsrcip(uint32_t dest /* nbo */) {
	printf("sr_integ_findsrcip(%d)\n",dest);
	return 0;
}

/**
 * Called by the transport layer for outgoing packets that need IP
 * encapsulation.
 *
 * @return 0 on success or waiting for MAC and then will try to send it,
 *         and 1 on failure.
 */
uint32_t sr_integ_ip_output(uint8_t* payload /* given */,
                            uint8_t  proto,
                            uint32_t src, /* nbo */
                            uint32_t dest, /* nbo */
                            int len) {
    struct sr_instance* sr;

    sr = sr_get_global_instance( NULL );

    const int offset = sizeof(packet_ethernet_t);
    const int totallen = len + sizeof(packet_ip4_t);
    byte packet[offset+totallen];
    memcpy(&packet[offset+sizeof(packet_ip4_t)], payload, len);

    packet_ip4_t * ip = (packet_ip4_t *) &packet[offset];

    packet_info_t pinfo;
    pinfo.packet = packet;
    pinfo.len = offset + totallen;
    pinfo.interface = NULL;
    pinfo.router = sr->interface_subsystem;

    ip->version = 4;
    ip->ihl = 5;
    ip->flags_fragmentoffset = htons(0x4000);
    ip->ttl = 64;
    ip->dscp_ecn = 0x10;
    ip->total_length = htons(totallen);
    ip->protocol = proto;
    ip->header_checksum = 0;
    ip->src_ip = src;
    ip->dst_ip = dest;
    ip->header_checksum = 0;
    ip->header_checksum = generatechecksum((unsigned short*) ip, sizeof(packet_ip4_t));

    ip_onreceive(&pinfo, ip);

    return 1;
}
