#ifndef GLOBALS_H_
#define GLOBALS_H_

#include "sr_router.h"
#include "packets.h"

int generatechecksum(unsigned short * buf, int len);
byte * malloc_udp_pseudo_header(packet_udp_t * udp, packet_ip4_t * ip, int * size);

#endif
