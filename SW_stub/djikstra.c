#include <stdio.h>
#include <limits.h>
#include <inttypes.h>

#include "packets.h"
#include "sr_router.h"
#include "globals.h"
#include "dataqueue.h"
#include "ethernet_packet.h"
#include "ip.h"
#include "unistd.h"

dataqueue_t rtable;

int minDistance(int dist[], bool sptSet[], int V) {
	int min = INT_MAX, min_index;

	int v;
	for (v = 0; v < V; v++)
		if (sptSet[v] == FALSE && dist[v] <= min)
			min = dist[v], min_index = v;

	return min_index;
}

void djikstra_recompute(router_t * router) {

	debug_println("--- Djikstra running! ---");

	dataqueue_t topology_t;
	queue_init(&topology_t);
	dataqueue_t * topology = &topology_t;

	dataqueue_t subnets_t;
	queue_init(&subnets_t);
	dataqueue_t * subnets = &subnets_t;

	addr_ip_t routers[router->num_interfaces];

	// Gather all entries in one dataqueue
	int i;
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

			if (queue_getidandlock(neighbours, n, (void **) &entry,
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
									sizeof(pwospf_lsa_dj_t));
						}

						d_link_t subnet;
						subnet.subnet = lsa_entry->subnet;
						subnet.netmask = lsa_entry->netmask;
						if (queue_existsunsafe(subnets, &subnet) == -1) {
							queue_add(subnets, &subnet, sizeof(d_link_t));
						}

					}

				} // else there is no LSU information available for that router yet

				queue_unlockid(neighbours, n);
				// entry is invalid from here on, whatever you do, do it inside
			}

		}

	}

	// Define and init the graph
	int s = subnets->size;
	int graph[s][s];
	int aa, bb;
	for (aa = 0; aa < subnets->size; aa++) {
		for (bb = 0; bb < subnets->size; bb++) {
			graph[aa][bb] = 0;
		}
	}

	int k;
	for (k = 0; k < topology->size; k++) {

		pwospf_lsa_dj_t * entry;
		int entry_size;
		if (queue_getidunsafe(topology, k, (void **) &entry, &entry_size)) {

			// Print all entries in the topology
			printf("Entry: ");
			printf("%s \t", quick_ip_to_string(entry->lsa.netmask));
			printf("%s \t", quick_ip_to_string(entry->lsa.subnet));
			printf("%s \t", quick_ip_to_string(entry->lsa.router_id));
			printf("%s \t \n", quick_ip_to_string(entry->router_ip));

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

						d_link_t link2;
						link2.netmask = entry_t->lsa.netmask;
						link2.subnet = entry_t->lsa.subnet;
						int id2 = queue_existsunsafe(subnets, &link2);

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
	for (q = 0; q < subnets->size; q++) {
		d_link_t * subnet;
		int entry_size;
		if (queue_getidunsafe(subnets, q, (void **) &subnet, &entry_size)
				!= -1) {
			printf("%s : %d\n", quick_ip_to_string(subnet->subnet), final[q]);
		} else {
			printf("No!");
		}
	}

	dataqueue_t rtable;
	queue_init(&rtable);
	dataqueue_t * rtable_old = &router->ip_table;

	for (q = 0; q < subnets->size; q++) {
		d_link_t * subnet;
		int entry_size;
		if (queue_getidunsafe(subnets, q, (void **) &subnet, &entry_size)
				!= -1) {
			ip_putintable(&rtable, subnet->subnet, &router->interface[intf[q]],
					subnet->netmask, TRUE, final[q], rips[q]);
		}
	}

	for (q = 0; q < queue_getcurrentsize(rtable_old); q++) {
		rtable_entry_t * entry_old;
		int entry_size_old;
		if (queue_getidandlock(rtable_old, q, (void **) &entry_old,
				&entry_size_old)) {

			assert(entry_size_old == sizeof(rtable_entry_t));

			if (!entry_old->dynamic) {
				ip_putintable(&rtable, entry_old->subnet, entry_old->interface,
						entry_old->netmask, entry_old->dynamic,
						entry_old->metric, entry_old->router_ip);
			}

			queue_unlockid(rtable_old, i);
		}
	}

	queue_free(&router->ip_table);
	router->ip_table = rtable;
	ip_print_table(&router->ip_table);

	queue_free(topology);
	queue_free(subnets);
}

