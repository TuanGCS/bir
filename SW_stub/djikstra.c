#include <stdio.h>
#include <limits.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "packets.h"
#include "sr_router.h"
#include "globals.h"
#include "dataqueue.h"
#include "ethernet_packet.h"
#include "ip.h"
#include "unistd.h"

dataqueue_t rtable;

#ifdef _CPUMODE_
#include "common/nf10util.h"
#include "reg_defines.h"

void update_hardwarearponeentry(router_t * router, int id, addr_ip_t ip,
		addr_ip_t netmask, addr_ip_t router_ip, int hw_oq) {
	if (id < 32) {

		writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_IP,
				ntohl(ip));
		writeReg(router->nf.fd,
		XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_IP_MASK, ntohl(netmask));
		writeReg(router->nf.fd,
		XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_NEXT_HOP_IP,
				ntohl(router_ip));
		writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_OQ,
				hw_oq);

		writeReg(router->nf.fd,
		XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_WR_ADDR, id);

		writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_IP,
				0);
		writeReg(router->nf.fd,
		XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_IP_MASK, 0);
		writeReg(router->nf.fd,
		XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_NEXT_HOP_IP, 0);
		writeReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_OQ,
				0);

		writeReg(router->nf.fd,
		XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_RD_ADDR, id);

		uint32_t read_ip, read_ip_mask, read_next_hop_ip, read_lpm_oq;
		readReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_IP,
				&read_ip);
		readReg(router->nf.fd,
		XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_IP_MASK, &read_ip_mask);
		readReg(router->nf.fd,
		XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_NEXT_HOP_IP,
				&read_next_hop_ip);
		readReg(router->nf.fd, XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_LPM_OQ,
				&read_lpm_oq);

		assert(read_ip == ntohl(ip));
		assert(read_ip_mask == ntohl(netmask));
		assert(read_next_hop_ip == ntohl(router_ip));
		assert(read_lpm_oq == hw_oq);
	}
}

void update_hardwarearp(router_t * router, dataqueue_t * table) {
	int i;

	for (i = 0; i < router->num_interfaces; i++)
		update_hardwarearponeentry(router, i, router->interface[i].ip,
				IP_CONVERT(255, 255, 255, 255), 0, router->interface[i].hw_id);

	for (i = 0; i < 32 - router->num_interfaces; i++) {
		rtable_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(table, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(rtable_entry_t));

			update_hardwarearponeentry(router, i + router->num_interfaces,
					entry->subnet, entry->netmask, entry->router_ip,
					entry->interface->hw_oq);

			queue_unlockid(table, i);
		} else {
			update_hardwarearponeentry(router, i + router->num_interfaces, 0, 0,
					0, 0);
		}
	}

}
#endif

int minDistance(int dist[], bool sptSet[], int V) {
	int min = INT_MAX, min_index;

	int v;
	for (v = 0; v < V; v++)
		if (sptSet[v] == FALSE && dist[v] <= min)
			min = dist[v], min_index = v;

	return min_index;
}

int cmpfunc(const void * a, const void * b) {
	const struct rtable_entry *p1 = a;
	const struct rtable_entry *p2 = b;
	if (p1->netmask <= p2->netmask) {
		return -1;
	} else {
		return 1;
	}
}

void djikstra_lock_router(router_t * router) {
	int i;
	for (i = 0; i < router->num_interfaces; i++)
		queue_lockall(&router->interface[i].neighbours);
}

void djikstra_unlock_router(router_t * router) {
	int i;
	for (i = 0; i < router->num_interfaces; i++)
		queue_unlockall(&router->interface[i].neighbours);
}

static pthread_mutex_t djkstra_locker = PTHREAD_MUTEX_INITIALIZER;
void djikstra_recompute(router_t * router) {

	pthread_mutex_lock( &djkstra_locker );
	djikstra_lock_router( router );

	debug_println("--- Djikstra running! ---");

	dataqueue_t topology_t;
	queue_init(&topology_t);
	dataqueue_t * topology = &topology_t;

	dataqueue_t subnets_t;
	queue_init(&subnets_t);
	dataqueue_t * subnets = &subnets_t;

	addr_ip_t routers[router->num_interfaces];

	int i;
	for (i = 0; i < router->num_interfaces; i++) {
		routers[i] = 0;
	}

	// Gather all entries in one dataqueue
	for (i = 0; i < router->num_interfaces; i++) {

		d_link_t subnet;
		subnet.subnet = router->interface[i].subnet_mask
				& router->interface[i].ip;
		subnet.netmask = router->interface[i].subnet_mask;

		pwospf_lsa_dj_t lsa_entry_t;
		lsa_entry_t.lsa.netmask = router->interface[i].subnet_mask;
		lsa_entry_t.lsa.subnet = router->interface[i].subnet_mask
				& router->interface[i].ip;
		lsa_entry_t.lsa.router_id = router->pw_router.router_id;
		lsa_entry_t.router_ip = 0;

		if (queue_existsunsafe(topology, &lsa_entry_t) == -1) {
			queue_add(topology, &lsa_entry_t, sizeof(pwospf_lsa_dj_t));
		}

		if (queue_existsunsafe(subnets, &subnet) == -1) {
			queue_add(subnets, &subnet, sizeof(d_link_t));
		}

		dataqueue_t * neighbours = &router->interface[i].neighbours;
		int n;
		for (n = 0; n < neighbours->size; n++) {
			pwospf_list_entry_t * entry;
			int entry_size;

			if (queue_getidunsafe(neighbours, n, (void **) &entry,
					&entry_size)) {

				assert(entry_size == sizeof(pwospf_list_entry_t));

				if (entry->lsu_lastcontents != NULL) {

					int j;
					for (j = 0; j < entry->lsu_lastcontents_count; j++) {
						pwospf_lsa_t * lsa_entry = &entry->lsu_lastcontents[j];
						pwospf_lsa_dj_t lsa_entry_dj;
						lsa_entry_dj.lsa = *lsa_entry;
						lsa_entry_dj.router_ip = entry->neighbour_ip;
						if (entry->immediate_neighbour == 1) {
							routers[i] = entry->neighbour_ip;
						}
						if (queue_existsunsafe(topology, &lsa_entry_dj) == -1) {
							queue_add(topology, &lsa_entry_dj,
									sizeof(pwospf_lsa_dj_t)); //TODO
						}

						d_link_t subnet;
						subnet.subnet = lsa_entry->subnet;
						subnet.netmask = lsa_entry->netmask;
						if (queue_existsunsafe(subnets, &subnet) == -1) {
							queue_add(subnets, &subnet, sizeof(d_link_t)); //TODO
						}

					}

				} // else there is no LSU information available for that router yet

			}

		}

	}

	// Define and init the graph
	int s = subnets->size;
	int graph[s][s];
	int aa, bb;
	for (aa = 0; aa < s; aa++) {
		for (bb = 0; bb < s; bb++) {
			graph[aa][bb] = 0;
		}
	}

	// Populate graph
	int k;
	for (k = 0; k < topology->size; k++) {

		pwospf_lsa_dj_t * entry;
		int entry_size;
		if (queue_getidunsafe(topology, k, (void **) &entry, &entry_size)) {

			// Print all entries in the topology
//			printf("Entry: ");
//			printf("%s \t", quick_ip_to_string(entry->lsa.netmask));
//			printf("%s \t", quick_ip_to_string(entry->lsa.subnet));
//			printf("%s \t", quick_ip_to_string(entry->lsa.router_id));
//			printf("%s \t \n", quick_ip_to_string(entry->router_ip));

			int p;
			for (p = k + 1; p < topology->size; p++) {

				pwospf_lsa_dj_t * entry_t;
				int entry_size_t;

				if (queue_getidunsafe(topology, p, (void **) &entry_t,
						&entry_size_t)) {

					if (entry->lsa.router_id == entry_t->lsa.router_id
							&& entry->lsa.subnet != entry_t->lsa.subnet) {

						d_link_t link1;
						link1.netmask = entry->lsa.netmask;
						link1.subnet = entry->lsa.subnet;
						int id1 = queue_existsunsafe(subnets, &link1);

						assert(id1 != -1);

						d_link_t link2;
						link2.netmask = entry_t->lsa.netmask;
						link2.subnet = entry_t->lsa.subnet;
						int id2 = queue_existsunsafe(subnets, &link2);

						assert(id2 != -1);

						graph[id1][id2] = 1;
						graph[id2][id1] = 1;

					}
				}

			}

		}
	}

	int dist[subnets->size];
	int final[subnets->size];
	int intf[subnets->size];
	addr_ip_t rips[subnets->size];
	bool sptSet[subnets->size];

	for (i = 0; i < subnets->size; i++) {
		final[i] = INT_MAX;
		intf[i] = INT_MAX;
	}

	for (i = 0; i < router->num_interfaces; i++) {

		int j;
		for (j = 0; j < subnets->size; j++) {
			dist[j] = INT_MAX;
			sptSet[j] = FALSE;
		}

		d_link_t subnet;
		subnet.netmask = router->interface[i].subnet_mask;
		subnet.subnet = router->interface[i].subnet_mask
				& router->interface[i].ip;
		int id = queue_existsunsafe(subnets, &subnet);

		assert(id != -1);

		dist[id] = 0;

		int k;
		for (k = 0; k < subnets->size; k++) {

			int u = minDistance(dist, sptSet, subnets->size);
			sptSet[u] = TRUE;

			int v;
			for (v = 0; v < subnets->size; v++) {

				if (!sptSet[v] && graph[u][v] && dist[u] != INT_MAX
						&& dist[u] + graph[u][v] < dist[v]) {
					dist[v] = dist[u] + graph[u][v];
				}
			}

		}

		for (k = 0; k < subnets->size; k++) {

			if (dist[k] < final[k]) {
				final[k] = dist[k];
				intf[k] = i;

				d_link_t * subnet;
				int entry_size;
				if (queue_getidunsafe(subnets, k, (void **) &subnet,
						&entry_size) != -1) {
					if (subnet->subnet
							== (router->interface[i].subnet_mask
									& router->interface[i].ip)) {
						rips[k] = 0;
					} else {
						rips[k] = routers[i];
					}
				}
			}
		}

	}

	int q;
//	for (q = 0; q < subnets->size; q++) {
//		d_link_t * subnet;
//		int entry_size;
//		if (queue_getidunsafe(subnets, q, (void **) &subnet, &entry_size)
//				!= -1) {
//			printf("%s : %d\n", quick_ip_to_string(subnet->subnet), final[q]);
//		} else {
//			printf("No!");
//		}
//	}

	dataqueue_t * rtable = &router->ip_table;

//	rtable_entry_t * entries = (rtable_entry_t *) malloc(
//			sizeof(rtable_entry_t) * subnets->size);
	rtable_entry_t * entries;
	int entries_size = 0;

	for (q = 0; q < subnets->size; q++) {

		if (intf[q] >= 0 && intf[q] < router->num_interfaces) {

			d_link_t * subnet;
			int entry_size;
			if (queue_getidunsafe(subnets, q, (void **) &subnet, &entry_size)
					!= -1) {

				if ((router->interface[intf[q]].ip
						& router->interface[intf[q]].subnet_mask)
								!= subnet->subnet) {

					if (entries_size == 0) {
						entries_size = 1;
						entries = (rtable_entry_t *) malloc(
								sizeof(rtable_entry_t));
					} else {
						entries_size++;
						entries = (rtable_entry_t *) realloc(entries, //tuk!!!1TODO
								sizeof(rtable_entry_t) * (entries_size));
					}

					rtable_entry_t * entry = &entries[entries_size - 1];

					assert(intf[q] >= 0 && intf[q] < router->num_interfaces);

					entry->interface = &router->interface[intf[q]];
					entry->dynamic = TRUE;
					entry->metric = final[q];
					entry->netmask = subnet->netmask;
					entry->router_ip = rips[q];
					entry->subnet = subnet->subnet;
				}

			}
		}
	}

	// LOCK
	queue_lockall(rtable);

	for (q = 0; q < queue_getcurrentsize(rtable); q++) {

		rtable_entry_t * entry_old;
		int entry_size_old;
		if (queue_getidunsafe(rtable, q, (void **) &entry_old,
				&entry_size_old)) {

			assert(entry_size_old == sizeof(rtable_entry_t));

			if (!entry_old->dynamic) {

				if (entries_size == 0) {
					entries_size = 1;
					entries = (rtable_entry_t *) malloc(sizeof(rtable_entry_t));
				} else {
					entries_size++;
					entries = (rtable_entry_t *) realloc(entries,
							sizeof(rtable_entry_t) * (entries_size));
				}

				memcpy(&entries[entries_size - 1], entry_old,
						sizeof(rtable_entry_t));

			}

		}
	}

	qsort(entries, entries_size, sizeof(rtable_entry_t), cmpfunc);

	queue_purge_unsafe(rtable);
	//queue_purge(rtable);

	for (q = 0; q < entries_size; q++) {
		if (queue_existsunsafe(rtable, &entries[q]) == -1) {
			queue_add_unsafe(rtable, &entries[q], sizeof(rtable_entry_t)); //todo
		}
	}
//		ip_putintable(rtable, entries[q].subnet, entries[q].interface,
//				entries[q].netmask, entries[q].dynamic, entries[q].metric,
//				entries[q].router_ip);

	queue_unlockall(rtable);
	// UNLOCK

	free(entries);

	//queue_free(&router->ip_table); // REMOVE
	//router->ip_table = rtable; // REMOVE
	ip_print_table(&router->ip_table);

	queue_free(topology);
	queue_free(subnets);

#ifdef _CPUMODE_
	update_hardwarearp(router, &router->ip_table);
#endif

	djikstra_unlock_router( router );
	pthread_mutex_unlock( &djkstra_locker );
}

