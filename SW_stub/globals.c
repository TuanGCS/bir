#include "globals.h"

int generatechecksum(unsigned short * buf, int len) {

	uint16_t * data = (uint16_t *) buf;
	int size = len / 2;

	uint32_t sum;
	for (sum = 0; size > 0; size--)
		sum += ntohs(*data++);
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);

	return htons(((uint16_t) ~sum));
}
