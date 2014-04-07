#!/usr/bin/env python

from NFTest import *
import sys
import os
from scapy.layers.all import Ether, IP, TCP

from reg_defines_reference_router import *

phy2loop0 = ('../connections/2phy', [])

nftest_init(sim_loop = [], hw_config = [phy2loop0])

nftest_start()

routerMAC = []
routerIP = []
for i in range(4):
    routerMAC.append("00:ca:fe:00:00:0%d"%(i+1))
    routerIP.append("192.168.%s.40"%i)

num_broadcast = 30

pktsb = []
for i in range(num_broadcast):
    if isHW():
        for port in range(2):
            pkt = make_IP_pkt(src_MAC="aa:bb:cc:dd:ee:ff", dst_MAC=routerMAC[0],
                      EtherType=0x800, src_IP="192.168.0.1",
                      dst_IP="192.168.1.1", pkt_len=100)
            pkt.time = ((i*(1e-8)) + (1e-6))
            pktsb.append(pkt)
            nftest_send_dma('nf%d'%port, pkt)
            nftest_expect_phy('nf%d'%port, pkt)

    if not isHW():
       pkt = make_IP_pkt(src_MAC="aa:bb:cc:dd:ee:ff", dst_MAC=routerMAC[0],
             EtherType=0x800, src_IP="192.168.0.1",
             dst_IP="192.168.1.1", pkt_len=100)
       pkt.time = ((i*(1e-8)) + (1e-6))
       pktsb.append(pkt)
       nftest_send_dma('nf0', pktsb)
       nftest_expect_phy('nf0', pktsb)


#pkt.show2()
nftest_barrier()

if isHW(): 	
    rres1=nftest_regread_expect(XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_PKT_SENT_FROM_CPU(), 60)
    mres=[rres1]
else:
    nftest_regread_expect(XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_PKT_SENT_FROM_CPU(), 30)
    mres=[]
mres=[]

nftest_finish(mres)
