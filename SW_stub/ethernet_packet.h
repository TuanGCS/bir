#ifndef ETHERNET_PACKET_H_
#define ETHERNET_PACKET_H_

#include "packets.h"

#include "sr_integration.h"
#include "sr_interface.h"

/* Send an ethernet packet. packet_start must point at the beginning of a valid ethernet packet!!!! */
void ethernet_packet_send(struct sr_instance* sr, interface_t* intf, addr_mac_t target_mac, addr_mac_t source_mac, uint16_t type, packet_info_t* pi);
void update_ethernet_header(packet_info_t* pi, addr_mac_t dst_mac, addr_mac_t src_mac);

#endif
