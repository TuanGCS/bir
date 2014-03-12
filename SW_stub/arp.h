#ifndef ARP_H_
#define ARP_H_

#include "sr_router.h"
#include "dataqueue.h"

#define ARP_OP_REQUEST (1)
#define ARP_OP_REPLY (2)

#define ARP_HTYPE_ETH (0x1)
#define ARP_PTYPE_IP (0x0800)

#define ARP_OPCODE_REQUEST (1)
#define ARP_OPCODE_REPLAY (2)

#define ARP_CACHE_TIMEOUT_REQUEST (120)
#define ARP_CACHE_TIMEOUT_BROADCAST (60)
#define ARP_CACHE_TIMEOUT_STATIC (-1)

#define ARPIP_QUEUE_TIMEOUT (2)
#define ARPIP_QUEUE_MAXARPREQUESTS (3)

#define ARP_THRESHOLD (32)

/* The function handles receiving arp packets.
 *  - pi is the packet info that contains the arpt able and incoming interface information
 *  - arp is the packet itself. It may be modified, after calling this function don't rely on arp being the same */
void arp_onreceive(packet_info_t* pi, packet_arp_t * arp);
void arp_putincache(router_t* router, dataqueue_t * cache, addr_ip_t ip, addr_mac_t mac, time_t timeout);
void arp_add_static(router_t * router, dataqueue_t * cache, addr_ip_t ip, addr_mac_t mac);
void arp_remove_ip_mac(router_t * router, dataqueue_t * cache, addr_ip_t ip, addr_mac_t mac);
void arp_send_request(router_t* router, interface_t* interface, addr_ip_t target_ip);
void arp_clear_all(router_t * router, dataqueue_t * cache);
void arp_clear_static(router_t * router, dataqueue_t * cache);
void arp_clear_dynamic(router_t * router, dataqueue_t * cache);
void arp_queue_ippacket_for_send_on_arp_request_response(packet_info_t * pi, interface_t * intf, addr_ip_t dest);
void arp_maintain_cache(void *arg);
void thread_arpipqueue_maintain(void *arg);

// returns position in dataqueue
int arp_getcachebymac(dataqueue_t * cache, addr_mac_t mac, arp_cache_entry_t * result);

// returns position in dataqueue
int arp_getcachebyip(dataqueue_t * cache, addr_ip_t ip, arp_cache_entry_t * result);

#endif
