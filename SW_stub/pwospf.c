#include "pwospf.h"

void pwospf_print(pwospf_packet_t * packet) {
	printf("------ PWOSPF (Pee-Wee Open Shortest Path First) PACKET -----\n");
	printf("packet->area_id=htonl(%d) //htonl(0x%x)\n", ntohl(packet->area_id), ntohl(packet->area_id));
	printf("packet->auth_data=htonl(%d) //htonl(0x%x)\n", ntohl(packet->auth_data), ntohl(packet->auth_data));
	printf("packet->auth_type=htonl(%d) //htonl(0x%x)\n", ntohl(packet->auth_type), ntohl(packet->auth_type));
	printf("packet->autotype=htons(%d) //htons(0x%x)\n", ntohs(packet->autotype), ntohs(packet->autotype));
	printf("packet->checksum=htons(%d) //htons(0x%x)\n", ntohs(packet->checksum), ntohs(packet->checksum));
	printf("packet->len=htons(%d)\n", ntohs(packet->len));
	printf("packet->router_id=%s //htonl(0x%x)\n", quick_ip_to_string(packet->router_id), ntohl(packet->router_id));
	printf("packet->type=%d //0x%x\n", packet->type, packet->type);
	printf("packet->version=%d //0x%x\n", packet->version, packet->version);
	printf("\n");
	fflush(stdout);
}

void pwospf_hello_print(pwospf_packet_hello_t * packet) {
	pwospf_print(&packet->pwospf_header);

	printf("****** HELLO part ******\n");
	printf("packet->net_mask=%s //htonl(0x%x)\n", quick_ip_to_string(packet->net_mask), ntohl(packet->net_mask));
	printf("packet->helloint=htons(%d)\n", ntohs(packet->helloint));
	printf("packet->padding=htons(%d) //htons(0x%x)\n", ntohs(packet->padding), ntohs(packet->padding));
	printf("\n");
	fflush(stdout);
}

void pwospf_onreceive_hello(packet_info_t * pi, pwospf_packet_hello_t * packet) {
	pwospf_hello_print(packet);
}

void pwospf_onreceive(packet_info_t* pi, pwospf_packet_t * packet) {
	switch(packet->type) {
	case OSPF_TYPE_HELLO:
		if (PACKET_CAN_MARSHALL(pwospf_packet_hello_t, sizeof(packet_ethernet_t)+sizeof(packet_ip4_t), pi->len)) {
			pwospf_packet_hello_t * pwospfhello = PACKET_MARSHALL(pwospf_packet_hello_t,pi->packet, sizeof(packet_ethernet_t)+sizeof(packet_ip4_t));
			pwospf_onreceive_hello(pi, pwospfhello);
		} else
			fprintf(stderr, "Invalid PWOSPF HELLO packet!\n");
		break;
	default:
		fprintf(stderr, "Unsupported PWOSPF type 0x%x\n", packet->type);
		break;
	}
}


