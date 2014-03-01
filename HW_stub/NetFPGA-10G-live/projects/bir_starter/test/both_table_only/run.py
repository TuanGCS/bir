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

nftest_regwrite(0x76800180,1000);
nftest_regwrite(0x76800184,0);
nftest_regwrite(0x76800188,0);
nftest_regread_expect(0x76800180,1000);




mres=[]

nftest_finish(mres)
