#ifndef IP_H_
#define IP_H_

#include "sr_router.h"
#include "packets.h"

void ip_onreceive(packet_info_t* pi, packet_ip4_t * ipv4);

#endif
