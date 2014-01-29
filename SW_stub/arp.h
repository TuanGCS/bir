#ifndef ARP_H_
#define ARP_H_

#include "sr_router.h"
#include "packets.h"

/* The function handles receiving arp packets.
 *  - pi is the packet info that contains the arpt able and incoming interface information
 *  - arp is the packet itself. It may be modified, after calling this function don't rely on arp being the same */
void arp_onreceive(packet_info_t* pi, packet_arp_t * arp);
void arp_putincache(arp_cache_t * cache, addr_ip_t ip, addr_mac_t mac, interface_t* interface, uint8_t timeout);
void arp_send_request(router_t* router, interface_t* interface, addr_ip_t target_ip);
arp_cache_entry_t * arp_getcachebymac(arp_cache_t * cache, addr_mac_t mac);
arp_cache_entry_t * arp_getcachebyip(arp_cache_t * cache, addr_ip_t ip);

#endif
