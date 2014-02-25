#ifndef PWOSPF_H_
#define PWOSPF_H_

#include "packets.h"
#include "sr_router.h"

#define ALLSPFRouters 0x50000e0

#define OSPF_VERSION (2)

#define OSPF_TYPE_HELLO (1)
#define OSPF_TYPE_LINK (4)

void pwospf_onreceive(packet_info_t* pi, pwospf_packet_t * packet);
pwospf_packet_hello_t* generate_pwospf_hello_header(addr_ip_t rid, addr_ip_t aid, uint32_t netmask);
pwospf_packet_link_t* generate_pwospf_link_header(addr_ip_t rid, addr_ip_t aid, uint32_t netmask, uint16_t advert);

#endif /* PWOSPF_H_ */
