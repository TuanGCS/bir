/* Filename: cli.c */

#include <signal.h>
#include <stdio.h>               /* snprintf()                        */
#include <stdlib.h>              /* malloc()                          */
#include <string.h>              /* strncpy()                         */
#include <sys/time.h>            /* struct timeval                    */
#include <unistd.h>              /* sleep()                           */
#include <stdarg.h>
#include "cli.h"
#include "cli_network.h"         /* make_thread()                     */
#include "cli_ping.h"            /* cli_ping_init(), cli_ping_request() */
#include "cli_trace.h"
#include "socket_helper.h"       /* writenstr()                       */
#include "../sr_base_internal.h" /* struct sr_instance                */
#include "../sr_common.h"        /* ...                               */
#include "../sr_router.h"        /* router_*()                        */
#include "../ip.h"
#include "../arp.h"

#define MAX_CHARS_IN_CLI_SEND_STRF (250)

#ifdef _CPUMODE_

	#define STR_HW_INFO_MAX_LEN (2048*5)
	#define STR_ARP_CACHE_MAX_LEN (2048*5)
	#define STR_INTFS_HW_MAX_LEN (2048*5)
	#define STR_RTABLE_MAX_LEN (2048*5)
	#define STR_GWTABLE_MAX_LEN (2048*5)

#endif /* _CPUMODE_ */

/** whether to shutdown the server or not */
static bool router_shutdown;

/** socket file descriptor where responses should be sent */
static int fd;

/** whether the fd is was terminated */
static bool fd_alive;

/** whether the client is in verbose mode */
static bool* pverbose;

/** whether to skip next prompt call */
static bool skip_next_prompt;

#ifdef _STANDALONE_CLI_
struct sr_instance* my_get_sr() {
    static struct sr_instance* sr = NULL;
    if( ! sr ) {
        sr = malloc( sizeof(*sr) );
        true_or_die( sr!=NULL, "malloc falied in my_get_sr" );

        router_t* subsystem = malloc( sizeof(router_t) );
        true_or_die( subsystem!=NULL, "Error: malloc failed in sr_integ_init" );
        /* router_init( subsystem ); */
        sr->interface_subsystem = subsystem;

        sr->topo_id = 0;
        strncpy( sr->vhost, "cli", SR_NAMELEN );
        strncpy( sr->server, "cli mode (no server)", SR_NAMELEN );
        strncpy( sr->user, "cli mode (no client)", SR_NAMELEN );
        if( gethostname(sr->lhost,  SR_NAMELEN) == -1 )
            strncpy( sr->lhost, "cli mode (unknown localhost)", SR_NAMELEN );

        sr_manual_read_intf_from_file( sr, "../config/interfaces" );
        sr->hw_init = 1;
        router_read_rtable_from_file( sr->interface_subsystem,
                                      "../config/rtable.test_arp_icmp" );
    }

    return sr;
}
#   define SR my_get_sr()
#else
#   include "../sr_integration.h" /* sr_get() */
#   define SR get_sr()
#endif
#define ROUTER SR->interface_subsystem

/**
 * Wrapper for writenstr.  Tries to send the specified string with the
 * file-scope fd.  If it fails, fd_alive is set to FALSE.  Does nothing if
 * fd_alive is already FALSE.
 */
static void cli_send_str( const char* str ) {
    if( fd_alive )
        if( 0 != writenstr( fd, str ) )
            fd_alive = FALSE;
}


/**
 * A printf-like implementation for sending over the network. Maximum size
 * to print is MAX_CHARS_IN_CLI_SEND_STRF. (Creator Martin)
 */
static void cli_send_strf( const char* format , ... ) {

	static char data[MAX_CHARS_IN_CLI_SEND_STRF];
	static pthread_mutex_t cli_sprintf_lock = PTHREAD_MUTEX_INITIALIZER;

    if( fd_alive ) {
    	va_list arg;
    	va_start (arg, format);

    	pthread_mutex_lock(&cli_sprintf_lock);
    	int size = vsnprintf(data, MAX_CHARS_IN_CLI_SEND_STRF, format, arg);
    	if (size < MAX_CHARS_IN_CLI_SEND_STRF && size >= 0) {
    		data[size] = 0;
    		if( 0 != writenstr( fd, data ) )
    			fd_alive = FALSE;
    	}

    	pthread_mutex_unlock(&cli_sprintf_lock);
    }
}


/**
 * Wrapper for writenstrs.  Tries to send the specified string(s) with the
 * file-scope fd.  If it fails, fd_alive is set to FALSE.  Does nothing if
 * fd_alive is already FALSE.
 */
static void cli_send_strs( int num_args, ... ) {
    const char* str;
    int ret;
    va_list args;

    if( !fd_alive ) return;
    va_start( args, num_args );

    ret = 0;
    while( ret==0 && num_args-- > 0 ) {
        str = va_arg(args, const char*);
        ret = writenstr( fd, str );
    }

    va_end( args );
    if( ret != 0 )
        fd_alive = FALSE;
}

void cli_init() {
    router_shutdown = FALSE;
    skip_next_prompt = FALSE;
    cli_ping_init();
    cli_traceroute_init();
}

bool cli_is_time_to_shutdown() {
    return router_shutdown;
}

bool cli_focus_is_alive() {
    return fd_alive;
}

void cli_focus_set( const int sfd, bool* verbose ) {
    fd_alive = TRUE;
    fd = sfd;
    pverbose = verbose;
}

void cli_send_help( cli_help_t help_type ) {
    if( fd_alive )
        if( !cli_send_help_to( fd, help_type ) )
            fd_alive = FALSE;
}

void cli_send_parse_error( int num_args, ... ) {
    const char* str;
    int ret;
    va_list args;

    if( fd_alive ) {
        va_start( args, num_args );

        ret = 0;
        while( ret==0 && num_args-- > 0 ) {
            str = va_arg(args, const char*);
            ret = writenstr( fd, str );
        }

        va_end( args );
        if( ret != 0 )
            fd_alive = FALSE;
    }
}

void cli_send_welcome() {
    cli_send_str( "Welcome to the bir router!\n" );
}

void cli_send_prompt() {
    if( !skip_next_prompt )
        cli_send_str( PROMPT );

    skip_next_prompt = FALSE;
}

void cli_show_all() {
#ifdef _CPUMODE_
    cli_show_hw();
    cli_send_str( "\n" );
#endif
    cli_show_ip();
#ifndef _CPUMODE_
#ifndef _MANUAL_MODE_
#ifndef MININET_MODE
    cli_send_str( "\n" );
    cli_show_vns();
#endif
#endif
#endif
}

#ifndef _CPUMODE_
void cli_send_no_hw_str() {
    cli_send_str( "HW information is not available when not in CPU mode\n" );
}
#else
#include "../common/nf10util.h"
#include "../reg_defines.h"

void cli_show_hw() {
    cli_send_str( "HW State:\n\n" );
    cli_show_hw_about();
    cli_show_hw_arp();
    cli_show_hw_intf();
    cli_show_hw_route();
}

void cli_show_hw_about() {
	router_t * router = ROUTER;
	cli_send_str( "MAC addresses:\nNo:\tMAC_HI_LO\tHW_ID\tHW_OQ\n" );

	int i;
	for (i = 0; i < router->num_interfaces; i++) {
		interface_t * intf = &router->interface[i];

		uint32_t mac_addr_low;
		uint32_t mac_addr_high;

		switch(intf->hw_id) {
		case INTF0:
			mac_addr_low = XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_0_LOW;
			mac_addr_high = XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_0_HIGH;
			break;
		case INTF1:
			mac_addr_low = XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_1_LOW;
			mac_addr_high = XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_1_HIGH;
			break;
		case INTF2:
			mac_addr_low = XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_2_LOW;
			mac_addr_high = XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_2_HIGH;
			break;
		case INTF3:
			mac_addr_low = XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_3_LOW;
			mac_addr_high = XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_3_HIGH;
			break;
		default:
			cli_send_strf( "%d: Unknown hardware id 0x%x\n", i, intf->hw_id);
			continue;
		}

		uint32_t read_mac_low;
		uint32_t read_mac_high;

		readReg(router->nf.fd, mac_addr_low, &read_mac_low);
		readReg(router->nf.fd, mac_addr_high, &read_mac_high);

		cli_send_strf( "%d:\t%04x%08x\t0x%x\t0x%x\n", i, read_mac_high, read_mac_low,  intf->hw_id, intf->hw_oq);
	}

	cli_send_str( "\n" );
}

void cli_show_hw_arp() {
	router_t * router = ROUTER;
	cli_send_str( "HW ARP registers:\nNo:\tIP\t\tMAC_HI_LO\n" );

	int i;
	for (i = 0; i < 32; i++) {
		writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_RD_ADDR, i);

		uint32_t read_ip, read_mac_low, read_mac_high;
		readReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_IP, &read_ip);
		readReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_MAC_HIGH, &read_mac_high);
		readReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_MAC_LOW, &read_mac_low);

		if (read_mac_low != 0 || read_mac_high != 0 || read_ip != 0)
			cli_send_strf( "%d:\t%s\t%04x%08x\n", i, quick_ip_to_string(htonl(read_ip)), read_mac_high, read_mac_low);
	}

	cli_send_str( "\n" );
}

void cli_show_hw_intf() {
	router_t * router = ROUTER;
	cli_send_str( "HW own interfaces:\nNo:\tIP\n" );

	int i;
	for (i = 0; i < 32; i++) {
		writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_ARP_RD_ADDR, i);

		uint32_t read_ip;
		writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_FILTER_RD_ADDR, i);
		readReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_FILTER_IP, &read_ip);

		if (read_ip != 0)
			cli_send_strf( "%d:\t%s\n", i, quick_ip_to_string(htonl(read_ip)));
	}

	cli_send_str( "\n" );
}

void cli_show_hw_route() {
	router_t * router = ROUTER;
	cli_send_str( "HW Longest Prefix Match table:\nNo:\tIP\t\tMASK\t\tNEXTIP\tOQ\n" );

	int i;
	for (i = 0; i < 32; i++) {
		writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_RD_ADDR, i);

		uint32_t read_ip, read_ip_mask, read_next_hop_ip, read_lpm_oq;
		readReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_IP, &read_ip);
		readReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_IP_MASK, &read_ip_mask);
		readReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_NEXT_HOP_IP, &read_next_hop_ip);
		readReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_OQ, &read_lpm_oq);


		if (read_ip != 0 || read_ip_mask != 0 || read_next_hop_ip != 0 || read_lpm_oq != 0) {
			cli_send_strf( "%d:\t%s", i, quick_ip_to_string(htonl(read_ip)));
			cli_send_strf( "\t%s", quick_ip_to_string(htonl(read_ip_mask)));
			cli_send_strf( "\t%s\t0x%x\n", quick_ip_to_string(htonl(read_next_hop_ip)), read_lpm_oq);
		}
	}

	cli_send_str( "\n" );
}
#endif

void cli_show_ip() {
    cli_send_str( "IP State:\n" );
    cli_show_ip_intf();
    cli_show_ip_route();
}

void cli_show_arp() {
	cli_show_ip_arp();
}

void cli_show_ip_arp() {
	dataqueue_t * cache = &ROUTER->arp_cache;

	struct timeval now;
	gettimeofday(&now, NULL);

	int i;
	for (i = 0; i < cache->size; i++) {
		arp_cache_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(cache, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(arp_cache_entry_t));
			char entrystr[100];
			if (entry->tv.tv_sec != -1)
				sprintf(entrystr, "%d.\t%s\t%s\t%ld\n", i,
						quick_mac_to_string(&entry->mac),
						quick_ip_to_string(entry->ip),
						entry->tv.tv_sec - now.tv_sec);
			else
				sprintf(entrystr, "%d.\t%s\t%s\tSTATIC\n", i,
						quick_mac_to_string(&entry->mac),
						quick_ip_to_string(entry->ip));

			queue_unlockid(cache, i);
			cli_send_strf(entrystr);
		}
	}
}

void cli_show_ip_intf() {
	char str_subnet[STRLEN_SUBNET];
	dataqueue_t * table = &ROUTER->ip_table;

	cli_send_strf("Destination \t Gateway \t Mask \t \t Interface \t Metric \n");
	int i;
	for (i = 0; i < table->size; i++) {
		rtable_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(table, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(rtable_entry_t));

			char entrystr[100];

		    subnet_to_string( str_subnet, entry->subnet, entry->netmask );

		    char subnet[16];
		    ip_to_string(subnet, entry->subnet);
		    char router_ip[16];
		    ip_to_string(router_ip, entry->router_ip);
		    char netmask[16];
		    ip_to_string(netmask, entry->netmask);

		    sprintf(entrystr, "%s \t %s \t %s \t %s \t %d \n", subnet, router_ip, netmask, entry->interface->name, entry->metric);

			queue_unlockid(table, i);
			// cli_send_strf will require locking of the queue
			// so if we do it before unlocking id, we will have a race condition!!!!
			cli_send_strf(entrystr);
		}
	}
}

void cli_show_ip_route() {
}

void cli_show_opt() {
    cli_show_opt_verbose();
}

void cli_show_opt_verbose() {
    if( *pverbose )
        cli_send_str( "Verbose: Enabled\n" );
    else
        cli_send_str( "Verbose: Disabled\n" );
}

void cli_show_ospf() {
    cli_send_str( "Neighbor Information:\n" );
    cli_show_ospf_neighbors();

    cli_send_str( "Topology:\n" );
    cli_show_ospf_topo();
}

void cli_show_ospf_neighbors() {
	router_t * router = ROUTER;

	int i;
	for (i = 0; i < router->num_interfaces; i++) {
		interface_t * intf = &router->interface[i];
		dataqueue_t * neighbours = &intf->neighbours;

		cli_send_strf( "%s:\n", intf->name );

		if (neighbours->size == 0)
			cli_send_str("\t<NO NEIGHBOURING ROUTERS>\n");
		else {
			int nid;
			for (nid = 0; nid < neighbours->size; nid++) {

				pwospf_list_entry_t * entry;
				int entry_size;
				if (queue_getidandlock(neighbours, nid, (void **) &entry, &entry_size)) {

					assert(entry_size == sizeof(pwospf_list_entry_t));

					cli_send_strf("\t%d: Id %d (0x%d)\tIP: %s\n", nid, entry->neighbour_id, entry->neighbour_id, quick_ip_to_string(entry->neighbour_ip));

					queue_unlockid(neighbours, nid);
				}
			}
		}
	}
}

void cli_show_ospf_topo() {
	router_t * router = ROUTER;

	int i;
	for (i = 0; i < router->num_interfaces; i++) {
		interface_t * intf = &router->interface[i];
		dataqueue_t * neighbours = &intf->neighbours;
		cli_send_strf( "Interface %s neighbours:\n", intf->name );

		int n;
		if (neighbours->size == 0)
			cli_send_str("  <NO NEIGHBOURING ROUTERS>\n");
		else for (n = 0; n < neighbours->size; n++) {
			pwospf_list_entry_t * entry;

			int entry_size;
			if (queue_getidandlock(neighbours, n, (void **) &entry, &entry_size)) {

				assert(entry_size == sizeof(pwospf_list_entry_t));

				if (entry->immediate_neighbour) {

					cli_send_strf("\n  * %d. Id %d (0x%x) \tIP: %s\n", n, entry->neighbour_id, entry->neighbour_id, quick_ip_to_string(entry->neighbour_ip));

					if (entry->lsu_lastcontents == NULL)
						cli_send_str("    <NO LINK STATE INFORMATION RECEIVED YET>\n");
					else {

						int j;
						for (j = 0; j < entry->lsu_lastcontents_count; j++) {
							pwospf_lsa_t * lsa_entry = &entry->lsu_lastcontents[j];

							cli_send_strf("    -> %d:\tMask:%s", j, quick_ip_to_string(lsa_entry->netmask));
							cli_send_strf("\tID:%s", quick_ip_to_string(lsa_entry->router_id));
							cli_send_strf("\tSub:%s\n", quick_ip_to_string(lsa_entry->subnet));

						}

					}

				} else
					cli_send_strf("\n  * An update was received about %d (0x%x) %s on this interface but the we are not immediately connected to it\n", n, entry->neighbour_id, entry->neighbour_id, quick_ip_to_string(entry->neighbour_ip));

				queue_unlockid(neighbours, n);
			}
		}

		cli_send_str("\n");
	}
}

#ifndef _VNS_MODE_
void cli_send_no_vns_str() {
#ifdef _CPUMODE_
    cli_send_str( "VNS information is not available when in CPU mode\n" );
#else
    cli_send_str( "VNS information is not available when in Manual mode\n" );
#endif
}
#else
void cli_show_vns() {
    cli_send_str( "VNS State:\n  Localhost: " );
    cli_show_vns_lhost();
    cli_send_str( "  Server: " );
    cli_show_vns_server();
    cli_send_str( "  Topology: " );
    cli_show_vns_topo();
    cli_send_str( "  User: " );
    cli_show_vns_user();
    cli_send_str( "  Virtual Host: " );
    cli_show_vns_vhost();
}

void cli_show_vns_lhost() {
    cli_send_strln( SR->lhost );
}

void cli_show_vns_server() {
    cli_send_strln( SR->server );
}

void cli_show_vns_topo() {
    char buf[7];
    snprintf( buf, 7, "%u\n", SR->topo_id );
    cli_send_str( buf );
}

void cli_show_vns_user() {
    cli_send_strln( SR->user );
}

void cli_show_vns_vhost() {
    cli_send_strln( SR->vhost );
}
#endif

void cli_manip_ip_arp_add( gross_arp_t* data ) {
	dataqueue_t * cache = &ROUTER->arp_cache;

	arp_add_static(ROUTER, cache, data->ip, data->mac);
	cli_show_ip_arp();
}

void cli_manip_ip_arp_del( gross_arp_t* data ) {
	dataqueue_t * cache = &ROUTER->arp_cache;

	arp_remove_ip_mac(ROUTER, cache, data->ip, data->mac);
	cli_show_ip_arp();
}

void cli_manip_ip_arp_purge_all() {
	arp_clear_all(ROUTER, &ROUTER->arp_cache);
}

void cli_manip_ip_arp_purge_dyn() {
	arp_clear_dynamic(ROUTER, &ROUTER->arp_cache);
}

void cli_manip_ip_arp_purge_sta() {
	arp_clear_static(ROUTER, &ROUTER->arp_cache);
}

void cli_manip_ip_intf_set( gross_intf_t* data ) {
    interface_t* intf;
    intf = router_lookup_interface_via_name( ROUTER, data->intf_name );
    if( intf ) {
        pthread_mutex_lock( &ROUTER->intf_lock );
        intf->ip = data->ip;
        intf->subnet_mask = data->subnet_mask;
        pthread_mutex_unlock( &ROUTER->intf_lock );
    }
    else
        cli_send_strs( 2, data->intf_name, " is not a valid interface\n" );
}

void cli_manip_ip_intf_set_enabled( const char* intf_name, bool enabled ) {
}

void cli_manip_ip_intf_down( gross_intf_t* data ) {
    cli_manip_ip_intf_set_enabled( data->intf_name, FALSE );
}

void cli_manip_ip_intf_up( gross_intf_t* data ) {
    cli_manip_ip_intf_set_enabled( data->intf_name, TRUE );
}

void cli_manip_ip_ospf_down() {
}

void cli_manip_ip_ospf_up() {
}

void cli_manip_ip_route_add( gross_route_t* data ) {
}

void cli_manip_ip_route_del( gross_route_t* data ) {
}

void cli_manip_ip_route_purge_all() {
}

void cli_manip_ip_route_purge_dyn() {
}

void cli_manip_ip_route_purge_sta() {
}

void cli_date() {
    char str_time[STRLEN_TIME];
    struct timeval now;

    gettimeofday( &now, NULL );
    time_to_string( str_time, now.tv_sec );
    cli_send_str( str_time );
}

void cli_exit() {
    cli_send_str( "Goodbye!\n" );
    fd_alive = FALSE;
}

bool cli_ping_handle_self( addr_ip_t ip ) {
    unsigned i;
    for( i=0; i<ROUTER->num_interfaces; i++ ) {
        if( ip == ROUTER->interface[i].ip ) {
            if( ROUTER->interface[i].enabled )
                cli_send_str( "Your interface is up.\n" );
            else
                cli_send_str( "Your interface is down.\n" );

            return TRUE;
        }
    }

    return FALSE;
}


void cli_ping( gross_ip_t* data ) {
    if( cli_ping_handle_self( data->ip ) )
        return;

    cli_ping_request( ROUTER, fd, data->ip );
    skip_next_prompt = TRUE;
}

void cli_ping_flood( gross_ip_int_t* data ) {
    int i;
    char str_ip[STRLEN_IP];

    if( cli_ping_handle_self( data->ip ) )
        return;

    ip_to_string( str_ip, data->ip );
    if( 0 != writenf( fd, "Will ping %s %u times ...\n", str_ip, data->count ) )
        fd_alive = FALSE;

    for( i=0; i<data->count; i++ )
        cli_ping_request( ROUTER, fd, data->ip );
    skip_next_prompt = TRUE;
}

void cli_shutdown() {
    cli_send_str( "Shutting down the router ...\n" );
    router_shutdown = TRUE;
    cli_ping_destroy();
    cli_traceroute_destroy();
    raise( SIGINT ); /* wake up cli_main thread blocked on accept() */
}

void cli_traceroute( gross_ip_t* data ) {
	cli_traceroute_request( ROUTER, fd, data->ip );
	skip_next_prompt = TRUE;
}

void cli_opt_verbose( gross_option_t* data ) {
    if( data->on ) {
        if( *pverbose )
            cli_send_str( "Verbose mode is already enabled.\n" );
        else {
            *pverbose = TRUE;
            cli_send_str( "Verbose mode is now enabled.\n" );
        }
    }
    else {
        if( *pverbose ) {
            *pverbose = FALSE;
            cli_send_str( "Verbose mode is now disabled.\n" );
        }
        else
            cli_send_str( "Verbose mode is already disabled.\n" );
    }
}
