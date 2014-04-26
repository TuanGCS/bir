#include <string.h>
#include <inttypes.h>

#include <stdio.h>
#include <stdlib.h>

#include <stdint.h>
#include <assert.h>
#include "packets.h"
#include "sr_router.h"
#include "dns.h"
#include "ip.h"
#include "globals.h"
#include "ethernet_packet.h"
#include <stdarg.h>

#include "sr_router.h"

#define MAX_DNS_FILENAME (300)
char dns_file_name[MAX_DNS_FILENAME];

void encode_dns_string(byte ** start, char ** names, int size) {
	int i;
	for (i = 0; i < size; i++) {
		const int len_name = strlen(names[i]);
		*((*start)++) = len_name;
		memcpy((*start), names[i], len_name);
		(*start)+=len_name;
	}
	*((*start)++) = 0;
}

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

dns_cache_entry_t * dns_mallocandparse_other_array(byte * address, int size) {
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
		curr->ttl = ntohs(*( (uint32_t *) address ) ); address += 4;
		curr->length = ntohs(*( (uint16_t *) address ) ); address += 2;
		curr->rdata  = (byte *) address;
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

dns_answer_proto_packet_t * dns_malloc_answer(uint16_t transact_id_networkorder) {
	dns_answer_proto_packet_t * answer = (dns_answer_proto_packet_t *) malloc(sizeof(dns_answer_proto_packet_t));
	answer->adding = 0;
	answer->totalsize = sizeof(packet_dns_t);
	answer->packet = malloc(answer->totalsize);
	memset(answer->packet, 0, answer->totalsize);
	packet_dns_t * dns = (packet_dns_t *) answer->packet;

	dns->totaladditionalrrs = 0;
	dns->totalanswerrrs = 0;
	dns->totalauthorityrrs = 0;
	dns->totalquestions = 0;
	dns->id = transact_id_networkorder;
	dns->QR = 1;

	return answer;
}

int calctotalsizeofencoding(char ** name, int name_count) {
	int total = 0;
	int i;
	for (i = 0; i < name_count; i++)
		total += strlen(name[i])+1;
	return total + 1;
}

void dns_answer_add_query_to_end(dns_answer_proto_packet_t * answer, dns_query_ho_t * question) {
	assert(answer->adding <= 0); // if this triggers order of adding Rdata is wrong
	answer->adding = 0;

	typedef struct PACKED {
		uint16_t type;
		uint16_t class;
	} dns_query_final_entry_t;

	const int totalsize = calctotalsizeofencoding(question->query_names, question->count) + sizeof(dns_query_final_entry_t);

	answer->totalsize += totalsize;
	answer->packet = realloc(answer->packet, answer->totalsize);
	byte * answerstartpos =  answer->packet + answer->totalsize - totalsize;
	packet_dns_t * dns = (packet_dns_t *) answer->packet;
	dns->totalquestions = htons(ntohs(dns->totalquestions) + 1);

	encode_dns_string(&answerstartpos, question->query_names, question->count);

	dns_query_final_entry_t * middle_entry = (dns_query_final_entry_t *) answerstartpos;
	answerstartpos+=sizeof(dns_query_final_entry_t);

	middle_entry->class = htons(question->qclass);
	middle_entry->type = htons(question->qtype);

	assert(answerstartpos == answer->packet + answer->totalsize);
}

byte * dns_answer_add_generic_answer_to_end(dns_answer_proto_packet_t * answer, char ** name, int name_count, uint32_t ttlseconds, uint16_t type, uint16_t class, uint16_t additional_data_size) {
	assert(answer->adding <= 0); // if this triggers order of adding Rdata is wrong
	answer->adding = 0;

	typedef struct PACKED {
		uint16_t type;
		uint16_t class;
		uint32_t ttl;
		uint16_t length;
	} dns_resource_basic_final_entry_t;

	const int totalsize = calctotalsizeofencoding(name, name_count) + sizeof(dns_resource_basic_final_entry_t) + additional_data_size;;

	answer->totalsize += totalsize;
	answer->packet = realloc(answer->packet, answer->totalsize);
	byte * answerstartpos =  answer->packet + answer->totalsize - totalsize;
	packet_dns_t * dns = (packet_dns_t *) answer->packet;
	dns->totalanswerrrs = htons(ntohs(dns->totalanswerrrs) + 1);

	encode_dns_string(&answerstartpos, name, name_count);

	dns_resource_basic_final_entry_t * middle_entry = (dns_resource_basic_final_entry_t *) answerstartpos;
	answerstartpos+=sizeof(dns_resource_basic_final_entry_t);

	middle_entry->class = htons(class);
	middle_entry->length = htons(additional_data_size);
	middle_entry->ttl = htonl(ttlseconds);
	middle_entry->type = htons(type);

	return answerstartpos;
}

void dns_answer_add_PTR_answer_to_end(dns_answer_proto_packet_t * answer, char ** name, int name_count, char ** answer_name, int answer_name_count, uint32_t ttlseconds) {
	byte * ptr_to_end = dns_answer_add_generic_answer_to_end(answer, name, name_count, ttlseconds, DNS_TYPE_PTR, DNS_CLASS_IN, calctotalsizeofencoding(answer_name, answer_name_count));

	encode_dns_string(&ptr_to_end, answer_name, answer_name_count);

	assert(ptr_to_end == answer->packet + answer->totalsize);
}

void dns_answer_add_A_answer_to_end(dns_answer_proto_packet_t * answer, char ** name, int name_count, addr_ip_t ip, uint32_t ttlseconds) {
	byte * ptr_to_end = dns_answer_add_generic_answer_to_end(answer, name, name_count, ttlseconds, DNS_TYPE_A, DNS_CLASS_IN, sizeof(addr_ip_t));

	*((addr_ip_t *)ptr_to_end) = ip; ptr_to_end+=sizeof(addr_ip_t);

	assert(ptr_to_end == answer->packet + answer->totalsize);
}

void send_dns_proto_response(packet_info_t * incoming_pi, dns_answer_proto_packet_t * answer, packet_udp_t * original, uint8_t dns_err_code) {
	router_t * router = incoming_pi->router;

	packet_info_t* pi = (packet_info_t *) malloc(sizeof(packet_info_t));

	pi->len = sizeof(packet_ethernet_t) + sizeof(packet_ip4_t) + sizeof(packet_udp_t) + answer->totalsize;
	pi->packet = (byte *) malloc(pi->len);
	pi->router = router;

	packet_ip4_t* ipv4 = (packet_ip4_t *) &pi->packet[sizeof(packet_ethernet_t)];
	packet_ip4_t* original_ipv4 = (packet_ip4_t *) &incoming_pi->packet[sizeof(packet_ethernet_t)];
	packet_ethernet_t * original_ethernet = (packet_ethernet_t *) incoming_pi->packet;

	packet_udp_t * udp = (packet_udp_t *) &pi->packet[sizeof(packet_ethernet_t) + sizeof(packet_ip4_t)];

	packet_dns_t * dns = (packet_dns_t *) answer->packet;

	dns->Rcode = dns_err_code;

	memcpy(&pi->packet[sizeof(packet_ethernet_t) + sizeof(packet_ip4_t) + sizeof(packet_udp_t)], answer->packet, answer->totalsize);

	udp->length = htons(sizeof(packet_udp_t) + answer->totalsize);
	udp->destination_port = original->source_port;
	udp->source_port = original->destination_port;
	udp->checksum = 0;

	// deal with updating the ip and ethernet and sending below

	pi->interface = incoming_pi->interface;

	generate_ipv4_header(pi->interface->ip, sizeof(packet_udp_t) + answer->totalsize, ipv4, IP_TYPE_UDP, original_ipv4->src_ip);

	ipv4->header_checksum = 0;
	ipv4->header_checksum = generatechecksum((unsigned short*) ipv4, sizeof(packet_ip4_t));

	int pseudodatasize;
	byte * pseudo_udp_data = malloc_udp_pseudo_header(udp, ipv4, &pseudodatasize);
	udp->checksum = generatechecksum((unsigned short*) pseudo_udp_data, pseudodatasize);
	free (pseudo_udp_data);

	if (ipv4->ttl < 1)
		return;

	if (ethernet_packet_send(get_sr(), pi->interface, original_ethernet->source_mac, pi->interface->mac,
			htons(ETH_IP_TYPE), pi) == -1)
		fprintf(stderr, "Cannot send PWOSPF LUS packet on interface %s\n",
				pi->interface->name);


	free(pi->packet);
	free(pi);
}

void dns_free_answer(dns_answer_proto_packet_t * answer) {
	free (answer->packet);
	free (answer);
}

char ** string_array_malloc(int n_args, ... ) {
	char ** answer = malloc(n_args * sizeof(char *));
	va_list ap;
	va_start(ap, n_args);
	int i;
	for (i=0;i<n_args;i++)
	{
		char * val=va_arg(ap,char *);
		const int size = strlen(val);

		answer[i] = malloc((size+1) * sizeof(char *));
		answer[i][size] = 0;
		memcpy(answer[i], val, size);
	}
	va_end(ap);
	return answer;
}

void string_array_free(int n_args, char ** array) {
	int i;
	for (i = 0; i < n_args; i++)
		free (array[i]);
	free (array);
}

addr_ip_t parse_ip(char ** array) {
	return IP_CONVERT(atoi(array[3]), atoi(array[2]), atoi(array[1]), atoi(array[0]));
}

void handle_question(packet_info_t* pi, packet_udp_t * udp, packet_dns_t * dns, dns_query_ho_t * question) {
	// a hardcoded answer that always returns 10.0.1.1 for testing purposes

	dns_answer_proto_packet_t * answer = dns_malloc_answer(dns->id);

	// ATTENTION! data should be added in the CORRECT ORDER to Rdata. First add QUERIES, THEN ANSWERS, THEN AUTHORITIES, THEN ADDITIONALS
	// If this order is not followed, an assertion will fail

	// ADD QUERIES
	dns_answer_add_query_to_end(answer, question);

	if (question->qclass != DNS_CLASS_IN) {
		fprintf(stderr, "Only class IN DNS queries are supported for now!\n");
		send_dns_proto_response(pi, answer, udp, DNS_ERROR_NOTIMPLEMENTED);
	}
	else if (question->qtype == DNS_TYPE_PTR) {

		// ADD ANSWERSr
		int err_code = DNS_ERROR_NO_ERROR;

		if (question->count < 4) {
			err_code = DNS_ERROR_NAMEERROR;
			goto ptr_send;
		}

		addr_ip_t ip = parse_ip(question->query_names);

		dns_db_entry_t entry;
		if (get_by_ip_safe(ip, &entry)) {

			dns_answer_add_PTR_answer_to_end(answer, question->query_names, question->count, entry.names, entry.count, 60*60*24);

		} else {
			err_code = DNS_ERROR_NAMEERROR;
			goto ptr_send;
		}

ptr_send:
	send_dns_proto_response(pi, answer, udp, err_code);

	}
	else if (question->qtype == DNS_TYPE_A) {

		// ADD ANSWERS
		dns_db_entry_t entry;
		if (get_by_domainname_array_safe(question->query_names, question->count, &entry)) {

			dns_answer_add_A_answer_to_end(answer, question->query_names, question->count, entry.rdata, 60*60*24);

			send_dns_proto_response(pi, answer, udp, DNS_ERROR_NO_ERROR);

		} else {
			send_dns_proto_response(pi, answer, udp, DNS_ERROR_NAMEERROR);
		}

	} else {
		fprintf(stderr, "Unsupported DNS query type!\n");
		send_dns_proto_response(pi, answer, udp, DNS_ERROR_NOTIMPLEMENTED);
	}

	dns_free_answer(answer);
}

void populate_database(dataqueue_t * dns_db, char * filename) {

	strncpy(dns_file_name, filename, MAX_DNS_FILENAME);

	char arra[256][256];
	char line[256];
	int size;
	int counter = 0;

	// Extract data from file
	FILE *file = fopen(filename, "r");

	if (file != NULL) {
		while(fgets(line, sizeof(line), file) != NULL) {
			strcpy(arra[counter], line);
			counter++;
		}
		fclose(file);
	} else {
		perror(filename);
	}

	int i;
	for (i = 0; i < counter; i++) {

		dns_db_entry_t db_entry;
		char * pch;
		pch = strtok(arra[i], " ");

		// Domain name
		int k = 0;
		char * tmp = pch;
		char domain[256];
		strcpy(domain, pch);

		// Count '.'
		while((tmp = strchr(tmp, '.')) != NULL) {
		    k++;
		    tmp++;
		}

		char ** answer = malloc((k+1) * sizeof(char *));

		char * dch;
		char *saveptr;
		int c = 0;
		dch = strtok_r(domain, ".", &saveptr);

		while (dch != NULL && c < (k+1)) {
			size = strlen(dch);
			answer[c] = malloc((size+1) * sizeof(char));
			answer[c][size] = 0;
			memcpy(answer[c], dch, size);
			c++;
			dch = strtok_r(NULL, ".", &saveptr);
		}

		db_entry.names = answer;
		db_entry.count = k+1;

		pch = strtok(NULL, " ");

		// Type
		db_entry.type = (uint16_t) atoi(pch);
		pch = strtok (NULL, " ");

		// Class
		db_entry.class = (uint16_t) atoi(pch);
		pch = strtok (NULL, " ");

		// Address/Data
		tmp = strchr(pch, '\n');
		strcpy(tmp, "\0");

		tmp = strtok_r(pch, ".", &saveptr);
		int ip[4];
		int j = 0;
		while (tmp != NULL && j < 4) {
			ip[j] = atoi(tmp);
			j++;
			tmp = strtok_r(NULL, ".", &saveptr);
		}

		db_entry.rdata = IP_CONVERT(ip[0], ip[1], ip[2], ip[3]);


		queue_add(dns_db, &db_entry, sizeof(db_entry));

		//free(answer);
	}

}

int get_by_domainname_array_safe(char ** dn, int dn_size, dns_db_entry_t * dest) {
	int i;
	dataqueue_t * dns_db = &get_router()->dns_db;

	for (i = 0; i < dns_db->size; i++) {
		dns_db_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(dns_db, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(dns_db_entry_t));

			if (dn_size == entry->count) {

				int j;

				int works = 1;
				for (j = 0; j < dn_size; j++)
					if (strcmp(dn[j], entry->names[j]) != 0)
						works = 0;

				if (works) {
					queue_unlockid(dns_db, i);
					*dest = *entry;
					return 1;
				}
			}

			queue_unlockid(dns_db, i);
		}
	}

	return 0;
}

int get_by_ip_safe(addr_ip_t ip, dns_db_entry_t * dest) {
	int i;
	dataqueue_t * dns_db = &get_router()->dns_db;

	for (i = 0; i < dns_db->size; i++) {
		dns_db_entry_t * entry;
		int entry_size;
		if (queue_getidandlock(dns_db, i, (void **) &entry, &entry_size)) {

			assert(entry_size == sizeof(dns_db_entry_t));

			if (ip == entry->rdata) {

				queue_unlockid(dns_db, i);
				*dest = *entry;
				return 1;
			}

			queue_unlockid(dns_db, i);
		}
	}

	return 0;
}

void dns_onreceive(packet_info_t* pi, packet_udp_t * udp, packet_dns_t * dns) {
	const uint16_t totalquestions = ntohs(dns->totalquestions);

	if (!dns->QR) {

		dns_query_ho_t * questions = dns_mallocandparse_query_array((byte *) ((byte *) dns + sizeof(packet_dns_t)), totalquestions);
		if (questions == NULL) {
			fprintf(stderr, "The DNS question cannot be parsed!");
			return;
		}

		// only answer to the first query for now
		handle_question(pi, udp, dns, &questions[0]);

		dns_free_query_array(questions, totalquestions);

	} else {

		//dns_query_ho_t * questions = dns_mallocandparse_query_array((byte *) ((byte *) dns + sizeof(packet_dns_t)), totalquestions);

//		dns_cache_entry_t * answers = dns_mallocandparse_other_array((byte *) ((byte *) dns + sizeof(packet_dns_t)), ntohs(dns->totalanswerrrs));
//		dns_cache_entry_t * auth_nameservers = dns_mallocandparse_other_array((byte *) ((byte *) dns + sizeof(packet_dns_t)), ntohs(dns->totalauthorityrrs));
//		dns_cache_entry_t * additional_records = dns_mallocandparse_other_array((byte *) ((byte *) dns + sizeof(packet_dns_t)), ntohs(dns->totaladditionalrrs));

	}
}

char * concat_names(char ** names, int count) {

	static char name[256];

	int name_size = 0;
	int j;
	for(j = 0; j < count; j++) {
		strncpy(&name[name_size], names[j], strlen(names[j]));
		name_size += strlen(names[j]);
		if(j != count -1) {
			name[name_size] = '.';
			name_size++;
		}

	}
	name[name_size] = 0;

	return name;
}

int dns_save_changes(void) {
	printf("Saving to file %s not yet implemented!\n", dns_file_name);

	FILE *file = fopen(dns_file_name, "w");
	dataqueue_t * dns_db = &get_router()->dns_db;

	if (file!=NULL)
	{
		int i;

		for (i = 0; i < dns_db->size; i++) {
			dns_db_entry_t * entry;
			int entry_size;
			if (queue_getidandlock(dns_db, i, (void **) &entry, &entry_size)) {

				assert(entry_size == sizeof(dns_db_entry_t));

				char * concated = concat_names(entry->names, entry->count);

				fprintf(file, "%s %d %d %s\n", concated, entry->type, entry->class, quick_ip_to_string(entry->rdata));

				queue_unlockid(dns_db, i);
			}
		}

		return fclose(file) == 0;
	}

	return 0;
}

