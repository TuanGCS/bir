#include <string.h>

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>
#include <assert.h>
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

dns_query_ho_t * dns_mallocandparse_other_array(byte * address, int size) {
	dns_cache_entry_t * array = (dns_cache_entry_t *) malloc(size * sizeof(dns_cache_entry_t));
	int id;
	for (id = 0; id < size; id++) {
		dns_cache_entry_t * curr = &array[id];
		curr->count = 0;
		curr->names = NULL;

		uint8_t char_size;
		while ((char_size = *(address++))!=0) {

			char * string = (char *) malloc(char_size+1);
			string[char_size] = 0;
			memcpy(string, address, char_size);
			curr->count++;

			if (curr->names == NULL)
				curr->names = (char **) malloc(sizeof(char *)*curr->count);
			else
				curr->names = (char **) realloc((void *) curr->names , sizeof(char *)*curr->count);

			curr->names[curr->count-1] = string;

			address+=char_size;
		}

		curr->type = ntohs(*( (uint16_t *)  address ) ); address += 2;
		curr->class = ntohs(*( (uint16_t *)  address ) ); address += 2;
		curr->ttl = ntohs(*( (uint16_t *) address ) ); address += 4;
		curr->length = ntohs(*( (uint16_t *) address ) ); address += 2;
		//curr->addr_ip = ntohs(*( (uint16_t *) address ) ); address += 4;
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

dns_answer_proto_packet_t * dns_malloc_answer(void) {
	dns_answer_proto_packet_t * answer = (dns_answer_proto_packet_t *) malloc(sizeof(dns_answer_proto_packet_t));
	answer->totalsize = sizeof(packet_dns_t);
	answer->packet = malloc(answer->totalsize);
	packet_dns_t * dns = (packet_dns_t *) answer->packet;


	// TODO! populate!
	dns->totaladditionalrrs = 0;
	dns->totalanswerrrs = 0;
	dns->totalauthorityrrs = 0;
	dns->totalquestions = 0;

	return answer;
}

int calctotalsizeofencoding(char ** name, int name_count) {
	int total = 0;
	int i;
	for (i = 0; i < name_count; i++)
		total += strlen(name[i])+1;
	return total + 1;
}

void dns_answer_add_A_answer_to_end(dns_answer_proto_packet_t * answer, char ** name, int name_count, addr_ip_t ip) {
	typedef struct dns_resource_middle_entry {
		uint16_t type;
		uint16_t class;
		uint32_t ttl;
		uint16_t length;
	} dns_resource_middle_entry_t;


	const int totalsize = calctotalsizeofencoding(name, name_count) + sizeof(dns_cache_entry_t) + sizeof(addr_ip_t);
	byte * answerstartpos = answer->packet + answer->totalsize;

	answer->totalsize += totalsize;
	answer->packet = realloc(answer->packet, answer->totalsize);
	packet_dns_t * dns = (packet_dns_t *) answer->packet;
	dns->totalanswerrrs++;

	int i;
	for (i = 0; i < name_count; i++) {
		const int len_name = strlen(name[i]);
		*(answerstartpos++) = len_name;
		memcpy(answerstartpos, name, len_name);
		answerstartpos+=len_name;
	}
	*(answerstartpos++) = 0;

	dns_resource_middle_entry_t * middle_entry = (dns_resource_middle_entry_t *) answerstartpos;
	answerstartpos+=sizeof(answerstartpos);

	//TODO!
	//middle_entry->class = htons();
	middle_entry->length = htons(sizeof(addr_ip_t));
	//middle_entry->ttl = htonl();
	middle_entry->type = htons(DNS_TYPE_A);

	*((addr_ip_t *) (answerstartpos++)) = ip;

	assert(answerstartpos == answer->packet + answer->totalsize);
}

void dns_free_answer(dns_answer_proto_packet_t * answer) {
	free (answer->packet);
	free (answer);
}

void dns_onreceive(packet_info_t* pi, packet_udp_t * udp, packet_dns_t * dns) {
	const uint16_t totalquestions = ntohs(dns->totalquestions);
	printf("Total questions %d\n", totalquestions);

	if (!dns->QR) {

		dns_query_ho_t * questions = dns_mallocandparse_query_array((byte *) ((byte *) dns + sizeof(packet_dns_t)), totalquestions);
		if (questions == NULL) {
			fprintf(stderr, "The DNS question cannot be parsed!");
			return;
		}

		int i;
		for (i = 0; i < totalquestions; i++) {
			int j;
			for (j = 0; j < questions[i].count; j++)
				printf("Qname %d: %s; ", j, questions[i].query_names[j]);
			printf("type %d, class %d\n", questions[i].qtype, questions[i].qclass);
		}

		dns_free_query_array(questions, totalquestions);

	} else {

		// Populate database

		dns_query_ho_t * questions = dns_mallocandparse_query_array((byte *) ((byte *) dns + sizeof(packet_dns_t)), totalquestions);
		dns_cache_entry_t * answers = dns_mallocandparse_query_array((byte *) ((byte *) dns + sizeof(packet_dns_t)), ntohs(dns->totalanswerrrs));
		dns_cache_entry_t * auth_nameservers = dns_mallocandparse_query_array((byte *) ((byte *) dns + sizeof(packet_dns_t)), ntohs(dns->totalauthorityrrs));
		dns_cache_entry_t * additional_records = dns_mallocandparse_query_array((byte *) ((byte *) dns + sizeof(packet_dns_t)), ntohs(dns->totaladditionalrrs));

	}
}

