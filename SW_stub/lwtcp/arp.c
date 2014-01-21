#include "arp.h"

void arp_onreceive(packet_ARP_t * arp, byte * payload, int payload_len) {
	// decision logic
	const int hardwaretype = ntohs(arp->hardwaretype);
	const int protocoltype = ntohs(arp->protocoltype);

	if (hardwaretype == ARP_HTYPE_ETH && protocoltype == ARP_PTYPE_IP && arp->hardwareaddresslength == 6 && arp->protocoladdresslength == 4) {
		const int opcode = ntohs(arp->opcode);
		switch(opcode) {
		case ARP_OPCODE_REQUEST:
			printf("ARP Request: Who has %d.%d.%d.%d? Tell %d.%d.%d.%d\n",
					arp->target_ip[0], arp->target_ip[1], arp->target_ip[2], arp->target_ip[3],
					arp->sender_ip[0], arp->sender_ip[1], arp->sender_ip[2], arp->sender_ip[3]);
			break;
		default:
			fprintf(stderr, "Unsupported ARP opcode %x!\n", opcode);
			break;
		}
	}
		else fprintf(stderr, "Unsupported ARP packet!\n");
}



