/*
 * pwospf.h
 *
 *  Created on: Feb 24, 2014
 *      Author: mininet
 */

#ifndef PWOSPF_H_
#define PWOSPF_H_

#include "packets.h"
#include "sr_router.h"

#define ALLSPFRouters 0x50000e0

#define OSPF_VERSION (2)

#define OSPF_TYPE_HELLO (1)
#define OSPF_TYPE_LINK (4)

void pwospf_onreceive(packet_info_t* pi, pwospf_packet_t * packet);

#endif /* PWOSPF_H_ */
