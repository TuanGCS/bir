#ifndef ARP_H_
#define ARP_H_

#include "sr_router.h"
#include "packets.h"

void arp_onreceive(packet_info_t* pi, packet_arp_t * arp, byte * payload, int payload_len);
arp_cache_entry_t * arp_getcachebymac(arp_cache_t * cache, addr_mac_t mac);
arp_cache_entry_t * arp_getcachebyip(arp_cache_t * cache, addr_ip_t ip);
void arp_putincache(arp_cache_t * cache, addr_ip_t ip, addr_mac_t mac, interface_t* interface);

#endif
