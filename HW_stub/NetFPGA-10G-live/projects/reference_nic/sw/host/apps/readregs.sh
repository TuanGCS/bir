#echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_0_LOW
#./rdaxi 0x76800004
#echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_0_HIGH
#./rdaxi 0x76800008
#echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_1_LOW
#./rdaxi 0x7680000c
#echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_1_HIGH
#./rdaxi 0x76800010
#echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_2_LOW
#./rdaxi 0x76800014
#echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_2_HIGH
#./rdaxi 0x76800018
#echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_3_LOW
#./rdaxi 0x7680001c
#echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_MAC_3_HIGH
#./rdaxi 0x76800020
echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_PKT_DROPPED_WRONG_DST_MAC
./rdaxi 0x76800024
echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_PKT_SENT_CPU_LPM_MISS
./rdaxi 0x76800028
echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_PKT_SENT_CPU_ARP_MISS
./rdaxi 0x7680002c
echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_PKT_SENT_CPU_NON_IP
./rdaxi 0x76800030
echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_PKT_DROPPED_CHECKSUM
./rdaxi 0x76800034
echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_PKT_FORWARDED
./rdaxi 0x76800038
echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_PKT_SENT_CPU_DEST_IP_HIT
./rdaxi 0x7680003c
echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_PKT_SENT_TO_CPU_BAD_TTL
./rdaxi 0x76800040
echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_PKT_SENT_CPU_OPTION_VER
./rdaxi 0x76800044
echo XPAR_NF10_ROUTER_OUTPUT_PORT_LOOKUP_0_PKT_SENT_FROM_CPU
./rdaxi 0x76800048
./rdaxi 0x7680004C
./rdaxi 0x76800050
./rdaxi 0x76800054
./rdaxi 0x7680005C
./rdaxi 0x76800060
./rdaxi 0x76800064
