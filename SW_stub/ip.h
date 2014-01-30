#ifndef IP_H_
#define IP_H_

#include "sr_router.h"
#include "packets.h"

#define IP_CONVERT(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))

void ip_onreceive(packet_info_t* pi, packet_ip4_t * ipv4);
void ip_putintable(ip_table_t * table, addr_ip_t ip, interface_t* interface);

#endif