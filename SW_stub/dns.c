#include <string.h>

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>
#include "packets.h"
#include "sr_router.h"
#include "dns.h"

dns_query_ho_t * dns_mallocandparse_query_array(byte * address, int size) {
	dns_query_ho_t * array = (dns_query_ho_t *) malloc(size * sizeof(dns_query_ho_t));
	int id;
	for (id = 0; id < size; id++) {
		dns_query_ho_t * curr = &array[id];
		curr->count = 0;
		curr->query_names = NULL;

		uint8_t char_size;
		while ((char_size = *(address++))!=0) {
			char * string = (char *) malloc(char_size+1);
			string[char_size] = 0;
			memcpy(string, address, char_size);
			curr->count++;

			if (curr->query_names == NULL)
				curr->query_names = (char **) malloc(sizeof(char *)*curr->count);
			else
				curr->query_names = (char **) realloc((void *) curr->query_names , sizeof(char *)*curr->count);

			curr->query_names[curr->count-1] = string;

			address+=char_size;
		}

		curr->qtype = ntohs(*( (uint16_t *)  address ) ); address += 2;
		curr->qclass = ntohs(*( (uint16_t *)  address ) ); address += 2;
	}
	return array;
}

void dns_free_query_array(dns_query_ho_t * questions, int size) {
	int i;
	for (i = 0; i < size; i++) {
		int j;
		for (j = 0; j < questions[i].count; j++)
			free (questions[i].query_names[j]);
		if (questions[i].query_names != NULL) free (questions[i].query_names);
	}
	free (questions);
}

void dns_onreceive(packet_info_t* pi, packet_udp_t * udp, packet_dns_t * dns) {
	const uint16_t totalquestions = ntohs(dns->totalquestions);
	printf("Total questions %d\n", totalquestions);

	if (!dns->QR) {

		dns_query_ho_t * questions = dns_mallocandparse_query_array((byte *) ((byte *) dns + sizeof(packet_dns_t)), totalquestions);

		int i;
		for (i = 0; i < totalquestions; i++) {
			int j;
			for (j = 0; j < questions[i].count; j++)
				printf("Qname %d: %s; ", j, questions[i].query_names[j]);
			printf("type %d, class %d\n", questions[i].qtype, questions[i].qclass);
		}

	} else {
		// TODO
	}

	dns_free_query_array(questions, totalquestions);
}

