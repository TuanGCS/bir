#ifndef ETHERNET_PACKET_H_
#define ETHERNET_PACKET_H_

#include "packets.h"

#include "sr_integration.h"
#include "sr_interface.h"

void ethernet_packet_send(struct sr_instance* sr, interface_t* intf, addr_mac_t target_mac, addr_mac_t source_mac, uint16_t type, byte * payload, int payloadsize);

#endif
