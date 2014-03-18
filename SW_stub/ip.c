#include "ip.h"
#include <stdio.h>
#include <netinet/in.h>

#include "sr_interface.h"
#include "sr_integration.h"
#include "globals.h"
#include "ethernet_packet.h"
#include "arp.h"
#include "packets.h"
#include "icmp_type.h"
#include "pwospf.h"
#include "cli/cli_ping.h"
#include "cli/cli_trace.h"

#include <string.h>

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>

// Move to header file. Should add be unique for every 2^16 - 1 packets then it will overflow and get back to 0 because it is unsigned
volatile uint16_t ip_id = 0;

void ip_print_table(dataqueue_t * table) {

	int i;
	printf("\nTHE IP TABLE\n-------------\n\n");
	for (i = 0; i < table->size; i++) {
		rtable_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(table, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(rtable_entry_t));

			printf("%d. IP: %s  ", i, quick_ip_to_string(entry->router_ip));
			printf("SUBNET: %s/", quick_ip_to_string(entry->subnet));
			printf("%s %d @ iface %s \n", quick_ip_to_string(entry->netmask),
					entry->metric, entry->interface->name);

			queue_unlockid(table, i);
		}
	}

	printf("\n");
}

void ip_print(packet_ip4_t * packet) {
	printf("------ IP PACKET -----\n");
	printf("packet->ihl=%d\n", packet->ihl);
	printf("packet->version=%d\n", packet->version);
	printf("packet->dscp_ecn=%d\n", packet->dscp_ecn);
	printf("packet->total_length=htons(%d)\n", ntohs(packet->total_length));
	printf("packet->id=htons(%d)\n", ntohs(packet->id));
	printf("packet->flags_fragmentoffset=htons(%d)\n",
			ntohs(packet->flags_fragmentoffset));
	printf("packet->ttl=%d\n", packet->ttl);
	printf("packet->protocol=%d\n", packet->protocol);
	printf("packet->header_checksum=htons(%d)\n",
			ntohs(packet->header_checksum));
	printf("packet->src_ip=\"%s\" //==htons(0x%x)==0x%x\n",
			quick_ip_to_string(packet->src_ip), ntohl(packet->src_ip),
			packet->src_ip);
	printf("packet->dst_ip=\"%s\" //==htons(0x%x)==0x%x\n",
			quick_ip_to_string(packet->dst_ip), ntohl(packet->dst_ip),
			packet->dst_ip);
	printf("\n");

	switch (packet->protocol) {
	case IP_TYPE_TCP: {
		packet_tcp_t * tcp = (packet_tcp_t *) (((byte *) packet)
				+ sizeof(packet_ip4_t));

		printf("tcp->source_port=htons(%d)\n", ntohs(tcp->source_port));
		printf("tcp->destination_port=htons(%d)\n",
				ntohs(tcp->destination_port));
		printf("tcp->sequence_number=htonl(%d)\n", ntohl(tcp->sequence_number));
		printf("tcp->ACK_number=htonl(%d)\n", ntohl(tcp->ACK_number));
		printf("tcp->offset=%d\n", tcp->offset);
		if (tcp->NS)
			printf("tcp->NS=1; //ECN-nonce concealment protection\n");
		if (tcp->CWR)
			printf("tcp->CWR=1; //Congestion Window Reduced\n");
		if (tcp->ECE)
			printf("tcp->ECE=1; //ECN-Echo\n");
		if (tcp->URG)
			printf("tcp->URG=1; //Urgent pointer field is significant\n");
		if (tcp->ACK)
			printf("tcp->ACK=1; //Acknowldegement field is significant\n");
		if (tcp->PSH)
			printf("tcp->PSH=1; //Push function\n");
		if (tcp->RST)
			printf("tcp->RST=1; //Reset TCP connection\n");
		if (tcp->SYN)
			printf("tcp->SYN=1; //Start TCP connection\n");
		if (tcp->FIN)
			printf("tcp->FIN=1; //Closing TCP connection\n");
		printf("tcp->window_size=htons(%d)\n", ntohs(tcp->window_size));
		printf("tcp->checksum=htons(%d)\n", ntohs(tcp->checksum));
		printf("tcp->urgent_pointer=htons(%d)\n", ntohs(tcp->urgent_pointer));

		int i;
		const int datasize = ntohs(packet->total_length) - sizeof(packet_ip4_t)
				- sizeof(packet_tcp_t) - tcp->offset;
		byte * data = ((byte *) packet) + sizeof(packet_ip4_t)
				+ sizeof(packet_tcp_t) + tcp->offset;

		printf("\n::: TCP DATA chars :::\n");
		for (i = 0; i < datasize; i++)
			printf("%c", (data[i] < ' ' || data[i] > '~') ? '.' : data[i]);
		printf("\n");

		printf("::: TCP DATA hex :::\n");
		for (i = 0; i < datasize; i++)
			printf("%02x ", data[i]);
		printf("\n");
	}
		break;
	default: {
		int i;
		const int datasize = ntohs(packet->total_length) - sizeof(packet_ip4_t);
		byte * data = ((byte *) packet) + sizeof(packet_ip4_t);

		printf("::: DATA chars :::\n");
		for (i = 0; i < datasize; i++)
			printf("%c", (data[i] < ' ' || data[i] > '~') ? '.' : data[i]);
		printf("\n");

		printf("::: DATA hex :::\n");
		for (i = 0; i < datasize; i++)
			printf("%02x ", data[i]);
		printf("\n");
	}
		break;
	}

	printf("\n");
	fflush(stdout);
}

// TODO! what happens if two interfaces match?
// returns id in table
int ip_longestprefixmatch(dataqueue_t * table, addr_ip_t ip,
		rtable_entry_t * result) {

	int maxmatch = 0;
	int answer = -1;

	int i;
	rtable_entry_t * entry;

	int entry_size;
	for (i = 0; i < table->size; i++) {
		if (queue_getidandlock(table, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(rtable_entry_t));

			//uint64_t mask = (1 >> (entry->netmask + 1)) - 1;

			const uint32_t tableip = entry->subnet;

			if ((ip & entry->netmask) == (tableip & entry->netmask)) {
				if (maxmatch < entry->netmask) {
					maxmatch = entry->netmask;
					answer = i;
					*result = *entry;
				}
			}

			queue_unlockid(table, i);
		}
	}

	return answer;
}

// returns id in table
int ip_directmatch(dataqueue_t * table, addr_ip_t ip, rtable_entry_t * result) {
	int i;
	for (i = 0; i < table->size; i++) {
		rtable_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(table, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(rtable_entry_t));

			if (entry->subnet == ip) {
				*result = *entry;
				queue_unlockid(table, i);
				return i;
			} else
				queue_unlockid(table, i);
		}
	}

	return -1;
}

void ip_putintable(dataqueue_t * table, addr_ip_t subnet,
		interface_t* interface, addr_ip_t netmask, bool dynamic, int metric,
		addr_ip_t ip) {

	rtable_entry_t existing; // memory allocation ;(

	int id = ip_directmatch(table, subnet, &existing);
	if (id >= 0) {
		if (existing.interface == interface)
			return; // nothing to do, this already exists
		else
			fprintf(stderr,
					"Possible IP conflict! %s was already registered at %s but now there is another %s that wants to be registered on %s!\n",
					quick_ip_to_string(existing.subnet),
					existing.interface->name, quick_ip_to_string(subnet),
					interface->name);
	}

	existing.router_ip = ip;
	existing.interface = interface;
	existing.subnet = subnet;
	existing.netmask = netmask;
	existing.dynamic = dynamic;
	existing.metric = metric;

	queue_add(table, (void *) &existing, sizeof(rtable_entry_t));

}

void sr_transport_input(uint8_t* packet /* borrowed */); // this function does the transport input to the system

void update_ip_packet_response(packet_info_t* pi, addr_ip_t dst_ip,
		addr_ip_t src_ip, uint8_t ttl) {

	packet_ip4_t* ipv4 = (packet_ip4_t *) &pi->packet[sizeof(packet_ethernet_t)];

	ipv4->dst_ip = dst_ip;
	ipv4->src_ip = src_ip;

	ipv4->ttl = ttl;
	ipv4->flags_fragmentoffset = 0;
	ipv4->id = htons(ip_id); // Fix
	ip_id++;

	ipv4->header_checksum = 0;
	ipv4->header_checksum = generatechecksum((unsigned short*) ipv4,
			sizeof(packet_ip4_t));

	packet_ethernet_t* eth = (packet_ethernet_t *) pi->packet;
	update_ethernet_header(pi, eth->source_mac, eth->dest_mac);
}

void generate_ipv4_header(addr_ip_t src_ip, int datagram_size,
		packet_ip4_t* ipv4) {

	ipv4->version = 4;
	ipv4->ihl = 5;
	ipv4->dscp_ecn = 0;
	ipv4->total_length = htons(sizeof(packet_ip4_t) + datagram_size);
	ipv4->id = 0;
	ipv4->flags_fragmentoffset = 0;
	ipv4->ttl = 65;
	ipv4->protocol = IP_TYPE_OSPF;
	ipv4->header_checksum = 0;
	ipv4->src_ip = src_ip;
	ipv4->dst_ip = ALLSPFRouters;

	ipv4->header_checksum = generatechecksum((unsigned short*) ipv4,
			sizeof(packet_ip4_t));

}

bool ip_header_check(packet_info_t* pi, packet_ip4_t * ipv4) {

	// Check if IP packet is verion 4
	if (ipv4->version != 4) {
		fprintf(stderr, "Not a IPv4 packet\n");
		return FALSE;
	}

	// Check if packet contains options
	if (ipv4->ihl != sizeof(packet_ip4_t) >> 2) {
		fprintf(stderr, "IPv4 packet contains options\n");
		return FALSE;
	}

	// Check if packet length is larger than the specified total length
	if (ipv4->total_length < sizeof(pi->packet[sizeof(packet_ethernet_t)])) {
		fprintf(stderr,
				"Invalid length of packet (received %lu, expected %d)\n",
				sizeof(pi->packet[sizeof(packet_ethernet_t)]),
				ipv4->total_length);
		return FALSE;
	}

	// Calculate and check header checksum
	if (generatechecksum((unsigned short*) ipv4, sizeof(packet_ip4_t)) != (0)) {
		fprintf(stderr, "IPv4 Header Checksum Error\n");
		return FALSE;
	}

	// Check if packet is a fragment -> Tested
	if ((ntohs(ipv4->flags_fragmentoffset) & 0x3FFF) != 0) {
		fprintf(stderr, "Packet is a fragment\n");
		return FALSE;
	}

	return TRUE;

}

void ip_onreceive(packet_info_t* pi, packet_ip4_t * ipv4) {

	// Check the validity of the IP header
	if (!ip_header_check(pi, ipv4)) {
		ip_print(ipv4);
		return;
	}

	if (ipv4->dst_ip == ALLSPFRouters && ipv4->protocol == IP_TYPE_OSPF) {

		if (PACKET_CAN_MARSHALL(pwospf_packet_t,
				sizeof(packet_ethernet_t)+sizeof(packet_ip4_t), pi->len)) {
			pwospf_packet_t * pwospf = PACKET_MARSHALL(pwospf_packet_t,
					pi->packet,
					sizeof(packet_ethernet_t) + sizeof(packet_ip4_t));
			pwospf_onreceive(pi, pwospf);
		} else
			fprintf(stderr, "Invalid PWOSPF packet!\n");
		return;
	}

	int i;
	for (i = 0; i < pi->router->num_interfaces; i++) {

		if (ipv4->dst_ip == pi->router->interface[i].ip) {

			if (ipv4->ttl <= 0) {
				fprintf(stderr, "Packet Time-To-Live is 0 or less for %s\n",
						pi->router->interface[i].name);
				icmp_type_time_exceeded(pi, ipv4, pi->router->interface[i].ip);

				return;
			}

			if (ipv4->protocol == IP_TYPE_ICMP) {

				packet_icmp_t* icmp =
						(packet_icmp_t *) &pi->packet[sizeof(packet_ethernet_t)
								+ sizeof(packet_ip4_t)];

				if (icmp->type == ICMP_TYPE_REQUEST) {

					icmp_type_echo_replay(pi, icmp);

					return;

				} else if (icmp->type == ICMP_TYPE_REPLAY) {
					if (!cli_ping_handle_reply(ipv4->src_ip, icmp->seq_num))
						icmp_send(pi, ipv4, pi->router->interface[i].ip, ICMP_TYPE_DST_UNREACH, ICMP_CODE_PORT_UNREACH);
					return;
				//} else if (icmp->type == ICMP_TYPE_DST_UNREACH) {
				//	cli_traceroute_handle_reply(ipv4->src_ip, icmp);
				//	return;
				//} else if (icmp->type == ICMP_TYPE_TIME_EXCEEDED) {
				//
				} else {
					fprintf(stderr,
							"Unknown type of ICMP %d (0x%x) was targeted at router interface %s\n",
							icmp->type, icmp->type,
							quick_ip_to_string(pi->router->interface[i].ip));
					return;
				}

			} else if (ipv4->protocol == IP_TYPE_TCP) {

				sr_transport_input((uint8_t *) ipv4);
				return;

			} else if (ipv4->protocol == IP_TYPE_OSPF) {
				if (PACKET_CAN_MARSHALL(pwospf_packet_t,
						sizeof(packet_ethernet_t)+sizeof(packet_ip4_t),
						pi->len)) {
					pwospf_packet_t * pwospf = PACKET_MARSHALL(pwospf_packet_t,
							pi->packet,
							sizeof(packet_ethernet_t) + sizeof(packet_ip4_t));
					pwospf_onreceive(pi, pwospf);
				} else
					fprintf(stderr, "Invalid PWOSPF packet was sent to us!\n");
				return;
			} else if (ipv4->protocol == IP_TYPE_UDP) {
				fprintf(stderr, "No UDP ports are used on the router\n");
				icmp_send(pi, ipv4, pi->router->interface[i].ip, ICMP_TYPE_DST_UNREACH, ICMP_CODE_PORT_UNREACH);

				return;
			} else {

				fprintf(stderr, "Unsupported IP packet type %d (0x%x)\n",
						ipv4->protocol, ipv4->protocol);
				icmp_send(pi, ipv4, pi->router->interface[i].ip, ICMP_TYPE_DST_UNREACH, ICMP_CODE_PROT_UNREACH);
				//icmp_type_dst_unreach(pi, ipv4, ICMP_CODE_PROT_UNREACH);
				return;

			}

		}
	}

	if (ipv4->ttl <= 1) {
		fprintf(stderr, "Packet Time-To-Live is 1 or less for %s\n",
				quick_ip_to_string(ipv4->dst_ip));
		icmp_type_time_exceeded(pi, ipv4, -1);

		return;
	}

	rtable_entry_t dest_ip_entry; // memory allocation ;(

	if (ip_longestprefixmatch(&pi->router->ip_table, ipv4->dst_ip,
			&dest_ip_entry) >= 0) {

		if (dest_ip_entry.router_ip == 0) {
			dest_ip_entry.router_ip = ipv4->dst_ip;
		}

		arp_cache_entry_t arp_dest; // memory allocation ;(

		if (arp_getcachebyip(&pi->router->arp_cache, dest_ip_entry.router_ip,
				&arp_dest) >= 0) {

			printf("%s\n", quick_mac_to_string(&arp_dest.mac));

			ipv4->ttl--;
			ipv4->header_checksum = 0;
			ipv4->header_checksum = generatechecksum((unsigned short*) ipv4,
					sizeof(packet_ip4_t));

			ethernet_packet_send(get_sr(), dest_ip_entry.interface,
					arp_dest.mac, dest_ip_entry.interface->mac,
					htons(ETH_IP_TYPE), pi);
		} else {
			fprintf(stderr,
					"IP packet will be queued upon ARP request response.\n");

			arp_queue_ippacket_for_send_on_arp_request_response(pi,
					dest_ip_entry.interface, dest_ip_entry.router_ip);

		}

	} else {
		printf("Longest prefix matching failed to find an interface for %s\n",
				quick_ip_to_string(ipv4->dst_ip));
		//ip_print_table(&pi->router->ip_table);

		icmp_send(pi, ipv4, pi->router->interface[i].ip, ICMP_TYPE_DST_UNREACH, ICMP_CODE_HOST_UNREACH);
		//icmp_type_dst_unreach(pi, ipv4, ICMP_CODE_HOST_UNREACH);
	}

}
