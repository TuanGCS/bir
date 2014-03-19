#ifndef ICMP_TYPE_H_
#define ICMP_TYPE_H_

#include "sr_router.h"

#define ICMP_TYPE_REPLAY (0)
#define ICMP_TYPE_DST_UNREACH (3)
#define ICMP_TYPE_REQUEST (8)
#define ICMP_TYPE_TIME_EXCEEDED (11)
#define ICMP_TYPE_TRACEROUTE (30)

#define ICMP_CODE_NET_UNREACH (0)
#define ICMP_CODE_HOST_UNREACH (1)
#define ICMP_CODE_PROT_UNREACH (2)
#define ICMP_CODE_PORT_UNREACH (3)
#define ICMP_CODE_DG_BIG (4)

void icmp_type_echo_replay(packet_info_t* pi, packet_icmp_t* icmp);
void icmp_type_time_exceeded(packet_info_t* pi, packet_ip4_t* ipv4, addr_ip_t src);
int icmp_allocate_and_send(router_t * rtr, addr_ip_t ip, int code, int type, int id, int count, byte * data, int data_size, addr_ip_t src);
void icmp_type_dst_unreach(packet_info_t* pi, packet_ip4_t* ipv4, int code);
void icmp_send(packet_info_t* pi, packet_ip4_t* ipv4, addr_ip_t src, int type, int code);

#endif
