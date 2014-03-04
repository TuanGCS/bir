#!/usr/bin/env python

from NFTest import *
import sys
import os
from scapy.layers.all import Ether, IP, TCP

phy2loop0 = ('../connections/2phy', [])

nftest_init(sim_loop = [], hw_config = [phy2loop0])

nftest_start()

routerMAC = []
routerIP = []
for i in range(4):
    routerMAC.append("00:ca:fe:00:00:0%d"%(i+1))
    routerIP.append("192.168.%s.40"%i)

num_broadcast = 20

pktsb = []
for i in range(num_broadcast):
    pkt = make_IP_pkt(src_MAC="aa:bb:cc:dd:ee:ff", dst_MAC=routerMAC[0],
                      EtherType=0x800, src_IP="192.168.0.1",
                      dst_IP="192.168.1.1", pkt_len=100)
    pkt.time = ((i*(1e-8)) + (1e-6))
    pktsb.append(pkt)
    if isHW():
        nftest_send_phy('nf0', pkt)
        nftest_expect_phy('nf1', pkt)

if not isHW():
    nftest_send_phy('nf0', pktsb)
    nftest_expect_phy('nf1', pktsb)


pkt.show2()
nftest_barrier()

pkts = []
for i in range(num_broadcast):
    pkt = make_IP_pkt(src_MAC="aa:bb:cc:dd:ee:ff", dst_MAC=routerMAC[0],
                      EtherType=0x800, src_IP="192.168.0.1",
                      dst_IP="192.168.1.1", pkt_len=100)
    pkt.time = ((i*(1e-8)) + (1e-6))
    pkt[IP].proto = 89
    pkts.append(pkt)
    if isHW():
        nftest_send_phy('nf0', pkt)
        nftest_expect_phy('nf1', pkt)

if not isHW():
    nftest_send_phy('nf0', pkts)
    nftest_expect_phy('nf1', pkts)

nftest_barrier()

pktsa = []
for i in range(num_broadcast):
    pkt = make_ARP_request_pkt(src_MAC="aa:bb:cc:dd:ee:ff", dst_MAC=routerMAC[0],
                      EtherType=0x806)
    pkt.time = ((i*(1e-8)) + (1e-6))
    pktsa.append(pkt)
    if isHW():
        nftest_send_phy('nf1', pkt)
        nftest_expect_phy('nf0', pkt)

if not isHW():
    nftest_send_phy('nf1', pktsa)
    nftest_expect_phy('nf0', pktsa)

nftest_barrier()

#nftest_regread_expect(0x76800000,2*num_broadcast);
#nftest_regread_expect(0x76800004,num_broadcast);
#nftest_regread_expect(0x76800008,num_broadcast);


mres=[]

nftest_finish(mres)
