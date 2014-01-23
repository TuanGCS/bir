#include "sr_router.h"
#include "ethernet_packet.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sr_integration.h"
#include "sr_interface.h"

void ethernet_packet_send(struct sr_instance* sr, interface_t* intf, addr_mac_t target_mac, addr_mac_t source_mac, uint16_t type, packet_info_t* pi){

	if (!PACKET_CAN_MARSHALL(packet_ethernet_t, 0, pi->len)) die("Invalid packet passed to ethernet_packet_sent! Internal bug!");

	packet_ethernet_t * ether = (packet_ethernet_t *) pi->packet;

	ether->dest_mac = target_mac;
	ether->source_mac = source_mac;
	ether->type = type;

	sr_integ_low_level_output(sr, pi->packet, pi->len, intf);
}


