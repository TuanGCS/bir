#include "ip.h"
#include <stdio.h>
#include <netinet/in.h>

#include "sr_interface.h"
#include "sr_integration.h"
#include "ethernet_packet.h"

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>


void ip_onreceive(packet_info_t* pi, packet_ip4_t * ipv4) {
	    printf("IPv4 packet:\n");
	    printf("Version: 0x%x \n", ipv4->version_ihl & 0xF);
	    printf("IHL: 0x%x \n", (ipv4->version_ihl >> 4) & 0xF);
	    printf("DSCP ECN: 0x%x \n", ipv4->dscp_ecn);
	    printf("Total length: %d \n", ntohs(ipv4->total_length));
	    printf("Id: %d \n", ntohs(ipv4->id));
	    printf("Flags fragment offset: 0x%x \n", ntohs(ipv4->flags_fragmentoffset));
	    printf("TTL: %d \n", ipv4->ttl);
	    printf("Protocol: %d (0x%x) \n", ipv4->protocol, ipv4->protocol);
	    printf("Headerchecksum: %d (0x%x)\n", ntohs(ipv4->header_checksum), ntohs(ipv4->header_checksum));
	    printf("Source ip: %s\n", quick_ip_to_string(ipv4->src_ip));
	    printf("Destination ip: %s\n", quick_ip_to_string(ipv4->dst_ip));

}

