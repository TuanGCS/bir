#ifndef ICMP_TYPE_H_
#define ICMP_TYPE_H_

#include "sr_router.h"

void icmp_type_echo_replay(packet_info_t* pi, packet_icmp_t* icmp);
void icmp_type_time_exceeded(packet_info_t* pi, packet_ip4_t* ipv4);
int icmp_allocate_and_send(router_t * rtr, addr_ip_t ip, int code, int type, int id, int count);

#endif
