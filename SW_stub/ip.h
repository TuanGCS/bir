#ifndef IP_H_
#define IP_H_

#include "sr_router.h"
#include "dataqueue.h"

#define IP_CONVERT(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))

void ip_onreceive(packet_info_t* pi, packet_ip4_t * ipv4);
void ip_putintable(dataqueue_t * table, addr_ip_t ip, interface_t* interface, int netmask);
int generatechecksum(unsigned short * buf, int len);

#endif
