

#ifndef CLI_TRACE_H_
#define CLI_TRACE_H_

#include "../sr_common.h"
#include "../sr_router.h"

/** ID field sent with outgoing pings */
#define PING_ID 3

/** maximum time the ping handler will block without considering new pings */
#define PING_HANDLER_MIN_RESPONSE_TIME_USEC 100000 /* 100ms */

/** default maximum time to wait for an echo reply */
#define PING_MAX_WAIT_FOR_REPLY_USEC 1000000 /* 1000ms */

/** Initialize the CLI ping handler */
void cli_traceroute_init();

/** Destroy the CLI ping handler */
void cli_traceroute_destroy();

/**
 * Send a ping request to IP and let fd know about the result.
 */
void cli_traceroute_request( router_t* rtr, int fd, addr_ip_t ip );

/** Handles an ICMP Echo Reply from ip with sequence number seq. */
void cli_traceroute_handle_reply( addr_ip_t src_ip, packet_icmp_t* icmp );

#endif /* CLI_TRACE_H_ */
