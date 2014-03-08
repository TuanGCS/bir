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

	// Gather all entries in one dataqueue
	int i;
	for (i = 0; i < router->num_interfaces; i++) {

		d_link_t subnet;
		subnet.subnet = router->interface[i].subnet_mask
				& router->interface[i].ip;
		subnet.netmask = router->interface[i].subnet_mask;

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
						queue_add(topology, lsa_entry, sizeof(pwospf_lsa_t));

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

//	// Print all subnets in the topology
	int q;
//	for (q = 0; q < subnets->size; q++) {
//		d_link_t * subnet;
//		int entry_size;
//		if (queue_getidunsafe(subnets, q, (void **) &subnet, &entry_size)
//				!= -1) {
//			printf("%s\n", quick_ip_to_string(subnet->subnet));
//		} else {
//			printf("No!");
//		}
//	}

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

		pwospf_lsa_t * entry;
		int entry_size;
		if (queue_getidunsafe(topology, k, (void **) &entry, &entry_size)) {

			// Print all entries in the topology
			printf("Entry: ");
			printf("%s \t", quick_ip_to_string(entry->netmask));
			printf("%s \t", quick_ip_to_string(entry->subnet));
			printf("%s \t \n", quick_ip_to_string(entry->router_id));

			int p;
			for (p = k + 1; p < topology->size; p++) {

				pwospf_lsa_t * entry_t;
				int entry_size_t;

				if (queue_getidunsafe(topology, p, (void **) &entry_t,
						&entry_size_t)) {

					if (entry->router_id == entry_t->router_id
							&& entry->subnet != entry_t->subnet) {

						graph[k][p] = 1;
						graph[p][k] = 1;

					}
				}

			}

		}
	}

	for (aa = 0; aa < subnets->size; aa++) {
		for (bb = 0; bb < subnets->size; bb++) {
			printf("%d ", graph[aa][bb]);
		}
		printf("\n");
	}

	int dist[subnets->size];
	bool sptSet[subnets->size];

	for (i = 0; i < subnets->size; i++) {
		dist[i] = INT_MAX;
		sptSet[i] = FALSE;
	}

	dist[1] = 0;

	for (i = 0; i < topology->size; i++) {

		int u = minDistance(dist, sptSet, subnets->size);
		sptSet[u] = TRUE;

		int v;
		for (v = 0; v < topology->size; v++) {

			if (!sptSet[v] && graph[u][v] && dist[u] != INT_MAX
					&& dist[u] + graph[u][v] < dist[v]) {
				dist[v] = dist[u] + graph[u][v];
			}
		}

	}

	for (q = 0; q < subnets->size; q++) {
		d_link_t * subnet;
		int entry_size;
		if (queue_getidunsafe(subnets, q, (void **) &subnet, &entry_size)
				!= -1) {
			printf("%s : %d\n", quick_ip_to_string(subnet->subnet), dist[q]);
		} else {
			printf("No!");
		}
	}

	queue_free(topology);
	queue_free(subnets);
}

