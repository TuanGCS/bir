#include "sr_router.h"
#include "ethernet_packet.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sr_integration.h"
#include "sr_interface.h"

void ethernet_packet_send(struct sr_instance* sr, interface_t* intf, addr_mac_t target_mac, addr_mac_t source_mac, uint16_t type, byte * payload, int payloadsize){
	const int totalsize = sizeof(packet_ethernet_t)+payloadsize;
	byte * data = (byte *) malloc(totalsize);

	packet_ethernet_t * ether = (packet_ethernet_t *) data;

	ether->dest_mac = target_mac;
	ether->source_mac = source_mac;
	ether->type = type;

	memcpy(&data[sizeof(packet_ethernet_t)], payload, payloadsize);

	int stat = sr_integ_low_level_output(sr, data, totalsize, intf);

	//int i = 0;
	//for (i = 0; i < totalsize; i++) printf("%x ", data[i]);
	//printf(" <-- Sent ethernet packet %d on interface %s\n",stat,intf->name);

	free(data);
}


