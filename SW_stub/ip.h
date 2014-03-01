#ifndef IP_H_
#define IP_H_

#include "sr_router.h"
#include "dataqueue.h"

#define IP_TYPE_ICMP (1)
#define IP_TYPE_TCP (6)
#define IP_TYPE_OSPF (89)

#define IP_CONVERT(a,b,c,d) (a | (b << 8) | (c << 16) | (d << 24))

void ip_onreceive(packet_info_t* pi, packet_ip4_t * ipv4);
void ip_putintable(dataqueue_t * table, addr_ip_t ip, interface_t* interface, int netmask, bool dynamic);
void update_ip_packet_response(packet_info_t* pi, addr_ip_t dst_ip, addr_ip_t src_ip, uint8_t ttl);
int ip_longestprefixmatch(dataqueue_t * table, addr_ip_t ip, ip_table_entry_t * result);
void ip_initialize_stub_packet(packet_info_t* pi, int protocol);
void ip_print(packet_ip4_t * packet);
void generate_ipv4_header(addr_ip_t src_ip, int datagram_size, packet_ip4_t* ipv4);

#endif
