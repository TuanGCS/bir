#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "pwospf.h"
#include "sr_router.h"
#include "packets.h"
#include "globals.h"
#include "dataqueue.h"
#include "ethernet_packet.h"
#include "ip.h"
#include "unistd.h"
#include "djikstra.h"
#include "arp.h"

#include "sr_thread.h"

void send_pwospf_lsa_packet(router_t* router);

volatile uint16_t hid = 0;

void pwospf_print(pwospf_packet_t * packet) {
	printf("------ PWOSPF (Pee-Wee Open Shortest Path First) PACKET -----\n");
	printf("packet->area_id=htonl(%d) //htonl(0x%x)\n", ntohl(packet->area_id),
			ntohl(packet->area_id));
	printf("packet->auth_data=htonl(%d) //htonl(0x%x)\n",
			ntohl(packet->auth_data), ntohl(packet->auth_data));
	printf("packet->auth_type=htonl(%d) //htonl(0x%x)\n",
			ntohl(packet->auth_type), ntohl(packet->auth_type));
	printf("packet->autotype=htons(%d) //htons(0x%x)\n",
			ntohs(packet->autotype), ntohs(packet->autotype));
	printf("packet->checksum=htons(%d) //htons(0x%x)\n",
			ntohs(packet->checksum), ntohs(packet->checksum));
	printf("packet->len=htons(%d)\n", ntohs(packet->len));
	printf("packet->router_id=%s //htonl(0x%x)\n",
			quick_ip_to_string(packet->router_id), ntohl(packet->router_id));
	printf("packet->type=%d //0x%x\n", packet->type, packet->type);
	printf("packet->version=%d //0x%x\n", packet->version, packet->version);
	printf("\n");
	fflush(stdout);
}

void pwospf_hello_print(pwospf_packet_hello_t * packet) {
	pwospf_print(&packet->pwospf_header);

	printf("****** HELLO part ******\n");
	printf("packet->net_mask=%s //htonl(0x%x)\n",
			quick_ip_to_string(packet->netmask), ntohl(packet->netmask));
	printf("packet->helloint=htons(%d)\n", ntohs(packet->helloint));
	printf("packet->padding=htons(%d) //htons(0x%x)\n", ntohs(packet->padding),
			ntohs(packet->padding));
	printf("\n");
	fflush(stdout);
}

// TODO! IMPORTANT! Does this lookup needs to be done via router_id or source ip??????
pwospf_list_entry_t * getneighbourfromidunsafe(dataqueue_t * neighbours,
		uint32_t router_id) {
	int n;
	for (n = 0; n < neighbours->size; n++) {
		pwospf_list_entry_t * entry;
		int entry_size;
		if (queue_getidunsafe(neighbours, n, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(pwospf_list_entry_t));

			if (entry->neighbour_id == router_id)
				return entry;
		}
	}
	return NULL;
}

void pwospf_reflood_to(packet_info_t * pi, interface_t * intf, addr_ip_t dest) {

	packet_ip4_t * ip = PACKET_MARSHALL(packet_ip4_t, pi->packet,
			sizeof(packet_ethernet_t));
	ip->dst_ip = dest;
	ip->src_ip = intf->ip;

	arp_cache_entry_t arp_dest; // memory allocation ;(
	if (arp_getcachebyip(&pi->router->arp_cache, dest, &arp_dest) >= 0) {

		ip->header_checksum = 0;
		ip->header_checksum = generatechecksum((unsigned short*) ip,
				sizeof(packet_ip4_t));

		if (ip->ttl < 1)
			return;

		if (ethernet_packet_send(get_sr(), intf, arp_dest.mac, intf->mac,
				htons(ETH_IP_TYPE), pi) == -1)
			fprintf(stderr, "Cannot send PWOSPF LUS packet on interface %s\n",
					intf->name);

	} else {
		fprintf(stderr,
				"PWOSPF ReFlood: IP packet will be queued upon ARP request response.\n");

		arp_queue_ippacket_for_send_on_arp_request_response(pi, intf, dest);

	}

}

int compare_lsus(pwospf_lsa_t * a, pwospf_lsa_t * b, int size) {
	int i, j;

	for (i = 0; i < size; i++) {
		pwospf_lsa_t * entry = &a[i];
		int discovered = 0;

		for (j = 0; j < size; j++) {
			pwospf_lsa_t * entry2 = &b[j];

			if (memcmp(entry, entry2, sizeof(pwospf_lsa_t)) == 0) {
				discovered = 1;
				break;
			}
		}

		if (!discovered)
			return 0;
	}

	return 1;
}

// excluded_neighbour points to a location in the dataqueue locked_neighbours
// for all other dataqueues, explicit lock must be used
void pwospf_reflood_packetpartiallyunsafe(packet_info_t * pi,
		pwospf_packet_link_t * packet, pwospf_list_entry_t * excluded_neighbour,
		dataqueue_t * locked_neighbours) {
	router_t * router = pi->router;

	int i;
	for (i = 0; i < router->num_interfaces; i++) {
		dataqueue_t * neighbours = &router->interface[i].neighbours;
		int n;
		for (n = 0; n < neighbours->size; n++) {
			pwospf_list_entry_t * entry;
			int entry_size;

			if (neighbours != locked_neighbours) {
				if (queue_getidandlock(neighbours, n, (void **) &entry,
						&entry_size)) {

					assert(entry_size == sizeof(pwospf_list_entry_t));

					debug_print("Reflooding PWOSPF LINK from %s ",
							quick_ip_to_string(
									excluded_neighbour->neighbour_ip));
					debug_println("to IP %s (%d entries)",
							quick_ip_to_string(entry->neighbour_ip),
							ntohl(packet->advert));
					pwospf_reflood_to(pi, &router->interface[i],
							entry->neighbour_ip);

					queue_unlockid(neighbours, n);
					// entry is invalid from here on, whatever you do, do it inside
				}
			} else {
				if (queue_getidunsafe(neighbours, n, (void **) &entry,
						&entry_size)) {

					assert(entry_size == sizeof(pwospf_list_entry_t));

					if (entry->neighbour_ip
							!= excluded_neighbour->neighbour_ip) {
						debug_print("Reflooding PWOSPF LINK from %s ",
								quick_ip_to_string(
										excluded_neighbour->neighbour_ip));
						debug_println("to IP %s (%d entries)",
								quick_ip_to_string(entry->neighbour_ip),
								ntohl(packet->advert));

						pwospf_reflood_to(pi, &router->interface[i],
								entry->neighbour_ip);
					}

				}
			}
		}
	}
}

void pwospf_onreceive_link(packet_info_t * pi, pwospf_packet_link_t * packet) {

	//printf("Received LINK from %s on interface %s\n", quick_ip_to_string(packet->pwospf_header.router_id), pi->interface->name);

	dataqueue_t * neighbours = &pi->interface->neighbours;
	packet_ip4_t * ip = PACKET_MARSHALL(packet_ip4_t, pi->packet,
			sizeof(packet_ethernet_t));
	pwospf_lsa_t * payload =
			(pwospf_lsa_t *) &pi->packet[sizeof(packet_ethernet_t)
					+ sizeof(packet_ip4_t) + sizeof(pwospf_packet_link_t)];
	const int payload_count = ntohl(packet->advert);

	if (pi->len
			< sizeof(packet_ethernet_t) + sizeof(packet_ip4_t)
					+ sizeof(pwospf_packet_link_t)
					+ payload_count * sizeof(pwospf_lsa_t)) {
		fprintf(stderr, "PWOSPF LINK packet received partially. Dropping...\n");
		return;
	}

	if (generatechecksum((unsigned short*) packet,
			sizeof(pwospf_packet_link_t) + payload_count * sizeof(pwospf_lsa_t))
			!= (0)) {
		fprintf(stderr, "PWOSPF LINK Header Checksum Error. Dropping...\n");
		return;
	}

	if (packet->pwospf_header.router_id == pi->router->pw_router.router_id) {
		fprintf(stderr,
				"PWOSPF LINK packet received from myself on %s from %s. Dropping...\n",
				pi->interface->name, quick_ip_to_string(ip->src_ip));
		return;
	}

	queue_lockall(neighbours);

	pwospf_list_entry_t * neighbour = getneighbourfromidunsafe(neighbours,
			packet->pwospf_header.router_id);
	if (neighbour != NULL) {

		// if we have some information about this neighbour
		if (neighbour->lsu_lastcontents != NULL
				&& neighbour->lsu_lastseq == packet->seq) {
			// if we have previously received LSUs from this neighbour and the sequence number of the packet matches
			queue_unlockall(neighbours);
			fprintf(stderr,
					"PWOSPF duplicate LINK packet received for %d. Dropping...\n",
					packet->pwospf_header.router_id);
			return;
		}

		// reflood packet to all neighbours, except neighbour itself
		pwospf_reflood_packetpartiallyunsafe(pi, packet, neighbour, neighbours);

		int g;
		for (g = 0; g < payload_count; g++)
			if (payload[g].router_id == 0)
				payload[g].router_id = packet->pwospf_header.router_id;

		// if a new packet is received for neighbour that we know about
		if (neighbour->lsu_lastcontents != NULL
				&& neighbour->lsu_lastcontents_count == payload_count) {
			// if we have previously received packets from this guy
			if (compare_lsus(neighbour->lsu_lastcontents, payload,
					neighbour->lsu_lastcontents_count)) {

				neighbour->lsu_lastseq = packet->seq; // this is the lastseq
				gettimeofday(&neighbour->lsu_timestamp, NULL); // update timestamp

				queue_unlockall(neighbours);
				fprintf(stderr,
						"PWOSPF The neighbour is sending the same LINK data as last time. Ignoring...\n");
				return;
			}
		}

		// if the state of the neighbour has changed, update database
		if (neighbour->lsu_lastcontents == NULL) {
			neighbour->lsu_lastcontents_count = payload_count;
			neighbour->lsu_lastcontents = (pwospf_lsa_t *) malloc(
					neighbour->lsu_lastcontents_count * sizeof(pwospf_lsa_t));
		} else if (neighbour->lsu_lastcontents_count != payload_count) {
			neighbour->lsu_lastcontents_count = payload_count;
			neighbour->lsu_lastcontents = (pwospf_lsa_t *) realloc(
					neighbour->lsu_lastcontents,
					neighbour->lsu_lastcontents_count * sizeof(pwospf_lsa_t));
		}

		// copy the topology data in each neighbour's lastcontents
		memcpy(neighbour->lsu_lastcontents, payload,
				payload_count * sizeof(pwospf_lsa_t));

		neighbour->lsu_lastseq = packet->seq; // this is the lastseq
		gettimeofday(&neighbour->lsu_timestamp, NULL); // update timestamp

		queue_unlockall(neighbours);

		debug_println(
				"An already existing neighbour sent a new PWOSPF LINK update (%d entries)!",
				payload_count);

		djikstra_recompute(pi->router);
		send_pwospf_lsa_packet(pi->router);

		return;
	}

	queue_unlockall(neighbours);

	int g;
	for (g = 0; g < payload_count; g++)
		if (payload[g].router_id == 0)
			payload[g].router_id = packet->pwospf_header.router_id;

	// if the neighbour is not in our database yet add it
	pwospf_list_entry_t newneighbour;
	newneighbour.neighbour_id = packet->pwospf_header.router_id;
	newneighbour.neighbour_ip = ip->src_ip;

	newneighbour.lsu_lastcontents_count = payload_count;
	newneighbour.lsu_lastcontents = (pwospf_lsa_t *) malloc(
			newneighbour.lsu_lastcontents_count * sizeof(pwospf_lsa_t));
	memcpy(newneighbour.lsu_lastcontents, payload,
			payload_count * sizeof(pwospf_lsa_t));

	newneighbour.lsu_lastseq = packet->seq; // this is the lastseq
	gettimeofday(&newneighbour.lsu_timestamp, NULL); // update timestamp

	newneighbour.immediate_neighbour = 0;

	queue_add(neighbours, &newneighbour, sizeof(pwospf_list_entry_t));

	debug_println("An new neighbour sent a PWOSPF LINK update (%d entries)!",
			payload_count);

	// flood the packet
	queue_lockall(neighbours);
	neighbour = getneighbourfromidunsafe(neighbours,
			packet->pwospf_header.router_id);
	if (neighbour != NULL) {
		for (g = 0; g < payload_count; g++)
			if (payload[g].router_id == packet->pwospf_header.router_id)
				payload[g].router_id = 0;
		pwospf_reflood_packetpartiallyunsafe(pi, packet, neighbour, neighbours);
	}
	queue_unlockall(neighbours);

	// recompute and tell everybody our table has changed
	djikstra_recompute(pi->router);
	send_pwospf_lsa_packet(pi->router);
}

void pwospf_onreceive_hello(packet_info_t * pi, pwospf_packet_hello_t * packet) {
	interface_t * interface = pi->interface;
	dataqueue_t * neighbours = &interface->neighbours;
	packet_ip4_t * ip = PACKET_MARSHALL(packet_ip4_t, pi->packet,
			sizeof(packet_ethernet_t));

	if (generatechecksum((unsigned short*) packet,
			sizeof(pwospf_packet_hello_t)) != (0)) {
		fprintf(stderr, "PWOSPF HELLO Header Checksum Error. Dropping...\n");
		return;
	}

	if (packet->netmask != interface->subnet_mask) {
		fprintf(stderr, "PWOSPF HELLO netmask does not match! Dropping...\n");
		return;
	}

	if (ntohs(packet->helloint) != interface->helloint) {
		fprintf(stderr,
				"PWOSPF HELLO helloint does not match (received %d, expected %d)! Dropping...\n",
				ntohs(packet->helloint), interface->helloint);
		return;
	}

	int was_immediate_neighbour = 0;
	int i;
	for (i = 0; i < neighbours->size; i++) {
		pwospf_list_entry_t * entry;
		int entry_size;
		int discovered = 0;
		if (queue_getidandlock(neighbours, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(pwospf_list_entry_t));

			if (entry->neighbour_ip == ip->src_ip) {
				discovered = 1;

				if (entry->neighbour_id != packet->pwospf_header.router_id) {
					debug_println(
							"OSPF: Router for %s has changed its id from %d to %d!",
							quick_ip_to_string(ip->src_ip), entry->neighbour_id,
							packet->pwospf_header.router_id);
					entry->neighbour_id = packet->pwospf_header.router_id;
				}

				entry->helloint = ntohs(packet->helloint);
				if (!entry->immediate_neighbour) {
					was_immediate_neighbour = 1;
				}
				entry->immediate_neighbour = 1;

				gettimeofday(&entry->timestamp, NULL);

			}

			queue_unlockid(neighbours, i);

		}

		if (was_immediate_neighbour) {
			djikstra_recompute(pi->router);
			send_pwospf_lsa_packet(pi->router);
		}

		if (discovered) {
			//debug_println("OSPF: Router id %d said HELLO :)", packet->pwospf_header.router_id);
			return;
		}
	}

	// this is a new neighbour, add it to the list!
	pwospf_list_entry_t newneighbour;
	newneighbour.neighbour_id = packet->pwospf_header.router_id;
	newneighbour.neighbour_ip = ip->src_ip;
	newneighbour.helloint = ntohs(packet->helloint);
	newneighbour.lsu_lastcontents = NULL;
	newneighbour.lsu_lastcontents_count = 0;
	newneighbour.immediate_neighbour = 1;
	gettimeofday(&newneighbour.timestamp, NULL);

	queue_add(neighbours, &newneighbour, sizeof(pwospf_list_entry_t));
	debug_println(
			"OSPF: A new neighbour was discovered on interface %s; Router id %d with ip %s.",
			pi->interface->name, newneighbour.neighbour_id,
			quick_ip_to_string(newneighbour.neighbour_ip));

	djikstra_recompute(pi->router);
	send_pwospf_lsa_packet(pi->router);
}

void pwospf_onreceive(packet_info_t* pi, pwospf_packet_t * packet) {

	if (packet->version != 2) {
		fprintf(stderr, "Unsupported OSPF version %d received. Dropping...\n",
				packet->version);
		return;
	}

	if (ntohl(packet->area_id) != pi->router->pw_router.area_id) {
		fprintf(stderr,
				"An OSPF packet was received from an unexpected area %d. Current router area is %d. Dropping...\n",
				ntohl(packet->area_id), pi->router->pw_router.area_id);
		return;
	}

	if (packet->auth_type != 0) {
		fprintf(stderr,
				"The received OSPF packet requires authentication which is currently unsupported. Dropping...\n");
		return;
	}

	switch (packet->type) {
	case OSPF_TYPE_HELLO:
		if (PACKET_CAN_MARSHALL(pwospf_packet_hello_t,
				sizeof(packet_ethernet_t)+sizeof(packet_ip4_t), pi->len)) {
			pwospf_packet_hello_t * pwospfhello = PACKET_MARSHALL(
					pwospf_packet_hello_t, pi->packet,
					sizeof(packet_ethernet_t) + sizeof(packet_ip4_t));
			pwospf_onreceive_hello(pi, pwospfhello);
		} else
			fprintf(stderr, "Invalid PWOSPF HELLO packet!\n");
		break;
	case OSPF_TYPE_LINK:
		if (PACKET_CAN_MARSHALL(pwospf_packet_link_t,
				sizeof(packet_ethernet_t)+sizeof(packet_ip4_t), pi->len)) {
			pwospf_packet_link_t * pwospflink = PACKET_MARSHALL(
					pwospf_packet_link_t, pi->packet,
					sizeof(packet_ethernet_t) + sizeof(packet_ip4_t));
			pwospf_onreceive_link(pi, pwospflink);
		} else
			fprintf(stderr, "Invalid PWOSPF LINK packet!\n");
		break;
	default:
		fprintf(stderr, "Unsupported PWOSPF type 0x%x\n", packet->type);
		break;
	}
}

void generate_pwospf_hello_header(addr_ip_t rid, addr_ip_t aid,
		uint32_t netmask, pwospf_packet_hello_t* pw_hello) {

	pw_hello->pwospf_header.version = OSPF_VERSION;
	pw_hello->pwospf_header.type = OSPF_TYPE_HELLO;
	pw_hello->pwospf_header.len = htons(sizeof(pwospf_packet_hello_t));
	pw_hello->pwospf_header.router_id = rid;
	pw_hello->pwospf_header.area_id = aid;
	pw_hello->pwospf_header.autotype = 0;
	pw_hello->pwospf_header.auth_type = 0;
	pw_hello->pwospf_header.auth_data = 0;
	pw_hello->netmask = netmask;
	pw_hello->helloint = htons(HELLOINT);
	pw_hello->padding = 0;

	pw_hello->pwospf_header.checksum = 0;
	pw_hello->pwospf_header.checksum = generatechecksum(
			(unsigned short*) pw_hello, sizeof(pwospf_packet_hello_t));

}

void generate_pwospf_link_header(addr_ip_t rid, addr_ip_t aid, uint32_t netmask,
		uint32_t advert, pwospf_packet_link_t* pw_link, int len) {

	pw_link->pwospf_header.version = OSPF_VERSION;
	pw_link->pwospf_header.type = OSPF_TYPE_LINK;
	pw_link->pwospf_header.len = htons(len);
	pw_link->pwospf_header.router_id = rid;
	pw_link->pwospf_header.area_id = aid;
	pw_link->pwospf_header.autotype = 0;
	pw_link->pwospf_header.auth_type = 0;
	pw_link->pwospf_header.auth_data = 0;
	pw_link->seq = htons(hid);
	hid++;
	pw_link->ttl = htons(64);
	pw_link->advert = htonl(advert);

	pw_link->pwospf_header.checksum = 0;
	pw_link->pwospf_header.checksum = generatechecksum(
			(unsigned short*) pw_link,
			sizeof(pwospf_packet_link_t) + advert * sizeof(pwospf_lsa_t));

}

// fills the stack with the neighbour ids to add
int get_topology(router_t * router, dataqueue_t * topology) {
	int count = 0;

	int i;
	for (i = 0; i < router->num_interfaces; i++) {

		// add info about interface
		pwospf_lsa_t intfentry;
		intfentry.netmask = router->interface[i].subnet_mask;
		intfentry.subnet = router->interface[i].subnet_mask
				& router->interface[i].ip;
		intfentry.router_id = 0;
		if (queue_existsunsafe(topology, &intfentry) == -1) {
			count++;
			queue_add(topology, &intfentry, sizeof(pwospf_lsa_t));
		}

		dataqueue_t * neighbours = &router->interface[i].neighbours;
		int n;
		for (n = 0; n < neighbours->size; n++) {
			pwospf_list_entry_t * entry;
			int entry_size;
			if (queue_getidandlock(neighbours, n, (void **) &entry,
					&entry_size)) {

				assert(entry_size == sizeof(pwospf_list_entry_t));

				if (entry->immediate_neighbour) {

					pwospf_lsa_t intfentry;
					intfentry.netmask = router->interface[i].subnet_mask;
					intfentry.subnet = router->interface[i].subnet_mask
							& router->interface[i].ip;
					intfentry.router_id = entry->neighbour_id;
					if (queue_existsunsafe(topology, &intfentry) == -1) {
						count++;
						queue_add(topology, &intfentry, sizeof(pwospf_lsa_t));
					}

				}

				queue_unlockid(neighbours, n);
			}
		}
	}

	return count;
}

void send_pwospf_lsa_packet(router_t* router) {

// since topology will be used by only one thread there is no need for locking/unlocking
	dataqueue_t topology;

	queue_init(&topology);
	const int topologysize = get_topology(router, &topology);

	int q;
	for (q = 0; q < topology.size; q++) {
		pwospf_lsa_t * entry;
		int entry_size;
		if (queue_getidunsafe(&topology, q, (void **) &entry, &entry_size)
				!= -1) {
		}
	}

	if (topologysize == 0) {
		printf("LSU will not be sent this time! Topology is empty!\n");
		queue_free(&topology);
		return;
	}

	packet_info_t* pi = (packet_info_t *) malloc(sizeof(packet_info_t));
	const int lsa_packet_length = sizeof(pwospf_packet_link_t)
			+ topologysize * sizeof(pwospf_lsa_t);
	pi->len = sizeof(packet_ethernet_t) + sizeof(packet_ip4_t)
			+ lsa_packet_length;
	pi->packet = (byte *) malloc(pi->len);
	pi->router = router;

	addr_ip_t aid = (addr_ip_t) 0; // Backbone
	addr_mac_t broadcast = MAC_BROADCAST;

	int i;
	for (i = 0; i < queue_getcurrentsize(&topology); i++) {
		int entry_size;
		pwospf_lsa_t * entry;
		assert(queue_getidunsafe(&topology, i, (void ** ) &entry, &entry_size));
		assert(entry_size == sizeof(pwospf_lsa_t));

		// we have entry i
		memcpy(
				&pi->packet[sizeof(packet_ethernet_t) + sizeof(packet_ip4_t)
						+ sizeof(pwospf_packet_link_t)
						+ i * sizeof(pwospf_lsa_t)], (void *) entry,
				sizeof(pwospf_lsa_t));
	}

	packet_ip4_t* ipv4 = (packet_ip4_t *) &pi->packet[sizeof(packet_ethernet_t)];
	pwospf_packet_link_t* pw_link =
			(pwospf_packet_link_t *) &pi->packet[sizeof(packet_ethernet_t)
					+ sizeof(packet_ip4_t)];

	for (i = 0; i < router->num_interfaces; i++) {
		pi->interface = &router->interface[i];

		generate_ipv4_header(router->interface[i].ip,
				lsa_packet_length, ipv4, IP_TYPE_OSPF, ALLSPFRouters);
		generate_pwospf_link_header(router->pw_router.router_id, aid,
				router->interface[i].subnet_mask, topologysize, pw_link,
				lsa_packet_length);

		if (ethernet_packet_send(get_sr(), &router->interface[i], broadcast,
				router->interface[i].mac, htons(ETH_IP_TYPE), pi) == -1)
			fprintf(stderr, "Cannot send PWOSPF LUS packet on interface %s\n",
					router->interface[i].name);
	}

	free(pi->packet);
	free(pi);

	printf("Send LSA (%d entries)\n", topologysize);

	gettimeofday(&router->last_lsu, NULL);

	queue_free(&topology);
}

void send_pwospf_hello_packet(router_t* router) {

	packet_info_t* pi = (packet_info_t *) malloc(sizeof(packet_info_t));
	pi->len = sizeof(packet_ethernet_t) + sizeof(packet_ip4_t)
			+ sizeof(pwospf_packet_hello_t);
	pi->packet = (byte *) malloc(pi->len);
	pi->router = router;

//		addr_ip_t aid = router->interface[i].ip
//				& router->interface[i].subnet_mask;
	addr_ip_t aid = (addr_ip_t) 0; // Backbone

	addr_mac_t broadcast = MAC_BROADCAST;

	packet_ip4_t* ipv4 = (packet_ip4_t *) &pi->packet[sizeof(packet_ethernet_t)];
	pwospf_packet_hello_t* pw_hello =
			(pwospf_packet_hello_t *) &pi->packet[sizeof(packet_ethernet_t)
					+ sizeof(packet_ip4_t)];

	int i;
	for (i = 0; i < router->num_interfaces; i++) {

		pi->interface = &router->interface[i];

		generate_ipv4_header(router->interface[i].ip,
				sizeof(pwospf_packet_hello_t), ipv4, IP_TYPE_OSPF, ALLSPFRouters);
		generate_pwospf_hello_header(router->pw_router.router_id, aid,
				router->interface[i].subnet_mask, pw_hello);

		int stat = ethernet_packet_send(get_sr(), &router->interface[i],
				broadcast, router->interface[i].mac, htons(ETH_IP_TYPE), pi);
		if (stat == -1)
			printf("Exception sending HELLO packet on interface %s\n",
					router->interface[i].name);

	}

	free(pi->packet);
	free(pi);

}

void removeallneighbourswithrouterid(router_t * router, pwospf_list_entry_t * toremove) {
	int j, i;
	for (j = 0; j < router->num_interfaces; j++) {

		interface_t * interface = &router->interface[j];
		dataqueue_t * neighbours = &interface->neighbours;

		for (i = 0; i < neighbours->size; i++) {
			pwospf_list_entry_t * entry;
			int entry_size;
			if (queue_getidandlock(neighbours, i, (void **) &entry,
					&entry_size)) {

				assert(entry_size == sizeof(pwospf_list_entry_t));

				if (entry->neighbour_id == toremove->neighbour_id)
					queue_unlockidandremove(neighbours, i);
				else
					queue_unlockid(neighbours, i);
			}

		}
	}
}

void pwospf_thread(void *arg) {
	router_t * router = (router_t *) arg;
	pthread_detach(pthread_self());

// starting up time
	sleep(HELLOINT);

	while (router->is_router_running) {
		send_pwospf_hello_packet(router);

		printf("Said HELLO to everyone\n");

		int changed = 0;
		struct timeval now;
		gettimeofday(&now, NULL);
		int j, i;
		for (j = 0; j < router->num_interfaces; j++) {

			interface_t * interface = &router->interface[j];
			dataqueue_t * neighbours = &interface->neighbours;

			for (i = 0; i < neighbours->size; i++) {
				pwospf_list_entry_t * entry;
				int entry_size;
				if (queue_getidandlock(neighbours, i, (void **) &entry,
						&entry_size)) {

					assert(entry_size == sizeof(pwospf_list_entry_t));

					if ((entry->immediate_neighbour && difftime(now.tv_sec, entry->timestamp.tv_sec)
									> 3 * entry->helloint)
							|| (entry->lsu_lastcontents != NULL
									&& difftime(now.tv_sec,
											entry->lsu_timestamp.tv_sec)
											> LSUINT_TIMEOUT)) {

						if (difftime(now.tv_sec, entry->timestamp.tv_sec)
								> 3 * entry->helloint)
							debug_println(
									"OSPF: Neighbour %d (%s) is dead (due to HELLO TIMEOUT) ;(\n",
									entry->neighbour_id,
									quick_ip_to_string(entry->neighbour_ip));
						else
							debug_println(
									"OSPF: Neighbour %d (%s) is dead (due to LINK TIMEOUT) ;(\n",
									entry->neighbour_id,
									quick_ip_to_string(entry->neighbour_ip));

						// release database
						free(entry->lsu_lastcontents); // TODO! TUK!

						changed = 1;

						pwospf_list_entry_t entry_copy = *entry;
						queue_unlockid(neighbours, i);

						removeallneighbourswithrouterid(router, &entry_copy);
					} else
						queue_unlockid(neighbours, i);
				}

			}
		}

		if (changed) {
			djikstra_recompute(router);
			send_pwospf_lsa_packet(router);
		}

		if (difftime(now.tv_sec, router->last_lsu.tv_sec) > LSUINT)
			send_pwospf_lsa_packet(router);

		sleep(HELLOINT);
	}
}

