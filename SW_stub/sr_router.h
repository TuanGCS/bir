/*
 * Filename: sr_router.h
 * Purpose:
 *   1) Handle each incoming packet
 *      -- Spawn new thread
 *      -- Pass to Ethernet handler
 *   2) Store state of router
 *      -- ARP Cache
 *      -- Interface List
 *      -- Routing Table
 */

#ifndef SR_ROUTER_H
#define SR_ROUTER_H

/* forward declarations */
struct router_t;

#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/time.h>
#include "common/nf10util.h"
#include "common/nf_util.h"
#include "reg_defines.h"
#include "sr_common.h"
#include "sr_interface.h"
#include "sr_work_queue.h"
#include "dataqueue.h"
#include "packets.h"

/** max number of interfaces the router max have */
#define ROUTER_MAX_INTERFACES 4

#define LSUINT 30
#define LSUINT_TIMEOUT 3*LSUINT

#define HELLOINT 10
#define NEIGHBOR_TIMEOUT 3*HELLOINT

/** router data structure */
typedef struct router_t {

    interface_t interface[ROUTER_MAX_INTERFACES];
    unsigned num_interfaces;
    pthread_mutex_t intf_lock;

    dataqueue_t arp_cache;
    dataqueue_t ip_table;
    dataqueue_t iparp_buffer;

    pwospf_router_t pw_router;

    bool use_ospf;

#ifdef _CPUMODE_
    struct nf_device nf;
    int	netfpga_regs;
#endif

#ifdef MININET_MODE
    char* name;      // name of router (e.g. r0)
#endif               // needed for iface initialisation

#ifndef _THREAD_PER_PACKET_
    work_queue_t work_queue;

#   ifndef NUM_WORKER_THREADS
#    define NUM_WORKER_THREADS 2 /* in addition to the main thread, ARP queue
                                    caretaker thread, and ARP cache GC thread */
#   endif
#endif
} router_t;

/** a packet along with the router and the interface it arrived on */
typedef struct packet_info_t {
    router_t* router;
    byte* packet;
    unsigned len;
    interface_t* interface;
} packet_info_t;

/** Initializes the router_t data structure. */
void router_init( router_t* router );

/** Destroys the router_t data structure. */
void router_destroy( router_t* router );

/**
 * Main entry function for a thread which is to handle the packet_info_t
 * specified by vpacket.  The thread will process the packet and then free the
 * buffers associated with the packet, including vpacket itself, before
 * terminating.
 */
void router_handle_packet( packet_info_t* pi );

#ifdef _THREAD_PER_PACKET_
/** Detaches the thread and then calls router_handle_packet with vpacket. */
void* router_pthread_main( void* vpacket );
#else
/** defines the different types of work which may be put on the work queue */
typedef enum work_type_t {
    WORK_NEW_PACKET,
} work_type_t;

/**
 * Entry point for worker threads doing work on the work queue.  Calls
 * routeR_handle_packet with the work->work field.
 */
void router_handle_work( work_t* work /* borrowed */ );
#endif

/**
 * Determines the interface to use in order to reach ip.
 *
 * @return interface to route from, or NULL if a route does not exist
 */
interface_t* router_lookup_interface_via_ip( router_t* router, addr_ip_t ip );

/**
 * Returns a pointer to the interface described by the specified name.
 *
 * @return interface, or NULL if the name does not match any interface
 */
interface_t* router_lookup_interface_via_name( router_t* router,
                                               const char* name );


/**
 * Adds an interface to the router.  Not thread-safe.  Should only be used
 * during the initialization phase.  The interface will be enabled by default.
 */
void router_add_interface( router_t* router,
                           const char* name,
                           addr_ip_t ip, addr_ip_t mask, addr_mac_t mac );

int packetinfo_ip_allocate(router_t* router, packet_info_t ** pinfo, int size, addr_ip_t dest, int protocol);

void packetinfo_free(packet_info_t ** pinfo);

#endif /* SR_ROUTER_H */
