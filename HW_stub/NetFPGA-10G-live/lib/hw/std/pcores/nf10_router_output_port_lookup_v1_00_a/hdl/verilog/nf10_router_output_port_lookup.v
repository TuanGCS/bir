/*******************************************************************************
 *
 *  NetFPGA-10G http://www.netfpga.org
 *
 *  File:
 *        nf10_output_port_lookup.v
 *
 *  Library:
 *        hw/std/pcores/nf10_router_output_port_lookup_v1_00_a
 *
 *  Module:
 *        nf10_output_port_lookup
 *
 *  Author:
 *        Adam Covington, Gianni Antichi
 *
 *  Description:
 *        Hardwire the hardware interfaces to CPU and vice versa
 *
 *  Copyright notice:
 *        Copyright (C) 2010, 2011 The Board of Trustees of The Leland Stanford
 *                                 Junior University
 *
 *  Licence:
 *        This file is part of the NetFPGA 10G development base package.
 *
 *        This file is free code: you can redistribute it and/or modify it under
 *        the terms of the GNU Lesser General Public License version 2.1 as
 *        published by the Free Software Foundation.
 *
 *        This package is distributed in the hope that it will be useful, but
 *        WITHOUT ANY WARRANTY; without even the implied warranty of
 *        MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *        Lesser General Public License for more details.
 *
 *        You should have received a copy of the GNU Lesser General Public
 *        License along with the NetFPGA source package.  If not, see
 *        http://www.gnu.org/licenses/.
 *
 */

module nf10_router_output_port_lookup
#(
    parameter C_FAMILY = "virtex5",
    parameter C_S_AXI_DATA_WIDTH = 32,
    parameter C_S_AXI_ADDR_WIDTH = 32,
    parameter C_USE_WSTRB = 0,
    parameter C_DPHASE_TIMEOUT = 0,
    parameter C_S_AXI_ACLK_FREQ_HZ = 100,
    parameter C_BASEADDR = 32'h76800000,
    parameter C_HIGHADDR = 32'h7680FFFF,
    parameter C_BASEADDR0 = 32'h76800180,
    parameter C_HIGHADDR0 = 32'h768001FF,
    parameter C_BASEADDR1 = 32'h76800100,
    parameter C_HIGHADDR1 = 32'h7680017F,
    parameter C_BASEADDR2 = 32'h76800080,
    parameter C_HIGHADDR2 = 32'h768000FF,
    parameter C_BASEADDR3 = 32'h76800000,
    parameter C_HIGHADDR3 = 32'h7680007F,
    //Master AXI Stream Data Width
    parameter C_M_AXIS_DATA_WIDTH=256,
    parameter C_S_AXIS_DATA_WIDTH=256,
    parameter C_M_AXIS_TUSER_WIDTH=128,
    parameter C_S_AXIS_TUSER_WIDTH=128,
    parameter SRC_PORT_POS=16,
    parameter DST_PORT_POS=24,
    parameter ARP_TABLE_DEPTH=32,
    parameter ROUTE_TABLE_DEPTH=32,
    parameter DST_IP_FILTER_TABLE_DEPTH=32
)
(
    // Global Ports
    input 				AXI_ACLK,
    input 				AXI_RESETN,

    // Master Stream Ports (interface to data path)
    output [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA,
    output [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB,
    output [C_M_AXIS_TUSER_WIDTH-1:0]M_AXIS_TUSER,
    output 				M_AXIS_TVALID,
    input  				M_AXIS_TREADY,
    output 				M_AXIS_TLAST,

    // Slave Stream Ports (interface to RX queues)
    input [C_S_AXIS_DATA_WIDTH-1:0] 	S_AXIS_TDATA,
    input [((C_S_AXIS_DATA_WIDTH/8))-1:0]S_AXIS_TSTRB,
    input [C_S_AXIS_TUSER_WIDTH-1:0] 	S_AXIS_TUSER,
    input  				S_AXIS_TVALID,
    output 				S_AXIS_TREADY,
    input  				S_AXIS_TLAST,

    // register port definitions

    input [C_S_AXI_ADDR_WIDTH-1:0]	S_AXI_AWADDR,
    input 				S_AXI_AWVALID,
    input [C_S_AXI_DATA_WIDTH-1:0] 	S_AXI_WDATA,
    input [C_S_AXI_DATA_WIDTH/8-1:0] 	S_AXI_WSTRB,
    input 				S_AXI_WVALID,
    input 				S_AXI_BREADY,
    input [C_S_AXI_ADDR_WIDTH-1:0] 	S_AXI_ARADDR,
    input 				S_AXI_ARVALID,
    input 				S_AXI_RREADY,
    output 				S_AXI_ARREADY,
    output [C_S_AXI_DATA_WIDTH-1:0] 	S_AXI_RDATA,
    output [1:0] 			S_AXI_RRESP,
    output 				S_AXI_RVALID,
    output 				S_AXI_WREADY,
    output [1:0] 			S_AXI_BRESP,
    output 				S_AXI_BVALID,
    output 				S_AXI_AWREADY

);

   function integer log2;
      input integer number;
      begin
         log2=0;
         while(2**log2<number) begin
            log2=log2+1;
         end
      end
   endfunction // log2

   // ------------ Internal Params --------
   localparam MODULE_HEADER = 0;
   localparam IN_PACKET     = 1;
   localparam NUM_WO_REGS	= 1;
   localparam NUM_RW_REGS       = 8;
   localparam NUM_RO_REGS       = 17;


   //------------- Wires ------------------
   wire  [C_M_AXIS_TUSER_WIDTH-1:0] tuser_fifo;
   reg 			  state, state_next;

  wire                                            Bus2IP_Clk;
  wire                                            Bus2IP_Resetn;
  wire     [C_S_AXI_ADDR_WIDTH-1 : 0]             Bus2IP_Addr;
  wire     [3:0]                                  Bus2IP_CS;
  wire                                            Bus2IP_RNW;
  wire     [C_S_AXI_DATA_WIDTH-1 : 0]             Bus2IP_Data;
  wire     [C_S_AXI_DATA_WIDTH/8-1 : 0]           Bus2IP_BE;
  reg      [C_S_AXI_DATA_WIDTH-1 : 0]             IP2Bus_Data;
  reg                                             IP2Bus_RdAck;
  reg                                             IP2Bus_WrAck;
  reg                                             IP2Bus_Error;
  
  wire     [NUM_WO_REGS*C_S_AXI_DATA_WIDTH-1 : 0] wo_regs;
  wire     [NUM_RW_REGS*C_S_AXI_DATA_WIDTH-1 : 0] rw_regs;
  wire     [NUM_RO_REGS*C_S_AXI_DATA_WIDTH-1 : 0] ro_regs;



   // ------------ Modules ----------------

   // -- AXILITE IPIF
  axi_lite_ipif_4bars #
  (
    .C_S_AXI_DATA_WIDTH (C_S_AXI_DATA_WIDTH),
    .C_S_AXI_ADDR_WIDTH (C_S_AXI_ADDR_WIDTH),
    .C_USE_WSTRB        (C_USE_WSTRB),
    .C_DPHASE_TIMEOUT   (C_DPHASE_TIMEOUT),
    .C_BAR0_BASEADDR    (C_BASEADDR0),
    .C_BAR0_HIGHADDR    (C_HIGHADDR0),
    .C_BAR1_BASEADDR    (C_BASEADDR1),
    .C_BAR1_HIGHADDR    (C_HIGHADDR1),
    .C_BAR2_BASEADDR    (C_BASEADDR2),
    .C_BAR2_HIGHADDR    (C_HIGHADDR2),
    .C_BAR3_BASEADDR    (C_BASEADDR3),
    .C_BAR3_HIGHADDR    (C_HIGHADDR3)
  ) axi_lite_ipif_inst
  (
    .S_AXI_ACLK          ( AXI_ACLK       ),
    .S_AXI_ARESETN       ( AXI_RESETN     ),
    .S_AXI_AWADDR        ( S_AXI_AWADDR   ),
    .S_AXI_AWVALID       ( S_AXI_AWVALID  ),
    .S_AXI_WDATA         ( S_AXI_WDATA    ),
    .S_AXI_WSTRB         ( S_AXI_WSTRB    ),
    .S_AXI_WVALID        ( S_AXI_WVALID   ),
    .S_AXI_BREADY        ( S_AXI_BREADY   ),
    .S_AXI_ARADDR        ( S_AXI_ARADDR   ),
    .S_AXI_ARVALID       ( S_AXI_ARVALID  ),
    .S_AXI_RREADY        ( S_AXI_RREADY   ),
    .S_AXI_ARREADY       ( S_AXI_ARREADY  ),
    .S_AXI_RDATA         ( S_AXI_RDATA    ),
    .S_AXI_RRESP         ( S_AXI_RRESP    ),
    .S_AXI_RVALID        ( S_AXI_RVALID   ),
    .S_AXI_WREADY        ( S_AXI_WREADY   ),
    .S_AXI_BRESP         ( S_AXI_BRESP    ),
    .S_AXI_BVALID        ( S_AXI_BVALID   ),
    .S_AXI_AWREADY       ( S_AXI_AWREADY  ),
	
	// Controls to the IP/IPIF modules
    .Bus2IP_Clk          ( Bus2IP_Clk     ),
    .Bus2IP_Resetn       ( Bus2IP_Resetn  ),
    .Bus2IP_Addr         ( Bus2IP_Addr    ),
    .Bus2IP_RNW          ( Bus2IP_RNW     ),
    .Bus2IP_BE           ( Bus2IP_BE      ),
    .Bus2IP_CS           ( Bus2IP_CS      ),
    .Bus2IP_Data         ( Bus2IP_Data    ),
    .IP2Bus_Data         ( IP2Bus_Data    ),
    .IP2Bus_WrAck        ( IP2Bus_WrAck   ),
    .IP2Bus_RdAck        ( IP2Bus_RdAck   ),
    .IP2Bus_Error        ( IP2Bus_Error   )
  );
 
//    assign IP2Bus_Data  =  Bus2IP_CS[1] ?     IP2Bus_Data0 : IP2Bus_Data1;    
//    assign IP2Bus_WrAck =  Bus2IP_CS[1] ?     IP2Bus_WrAck0 : IP2Bus_WrAck1;   
//    assign IP2Bus_RdAck =  Bus2IP_CS[1] ?      IP2Bus_RdAck0 : IP2Bus_RdAck1;  
//    assign IP2Bus_Error =  Bus2IP_CS[1] ?      IP2Bus_Error0 : IP2Bus_Error1;  

  wire [C_S_AXI_DATA_WIDTH - 1:0]  IP2Bus_Data0,IP2Bus_Data1,IP2Bus_Data2,IP2Bus_Data3;
  wire [C_S_AXI_DATA_WIDTH*3 - 1:0] tbl_rd_data3, tbl_wr_data3;
  wire [4:0] tbl_rd_addr3, tbl_wr_addr3;
  wire [C_S_AXI_DATA_WIDTH*4 - 1:0] tbl_rd_data2, tbl_wr_data2;
  wire [4:0] tbl_rd_addr2, tbl_wr_addr2;
  wire [31:0] tbl_rd_data1, tbl_wr_data1;
  wire [4:0] tbl_rd_addr1, tbl_wr_addr1;



  // -- IPIF REGS
  ipif_regs #
  (
    .C_S_AXI_DATA_WIDTH (C_S_AXI_DATA_WIDTH),          
    .C_S_AXI_ADDR_WIDTH (C_S_AXI_ADDR_WIDTH),   
    .NUM_WO_REGS	(NUM_WO_REGS),
    .NUM_RW_REGS        (NUM_RW_REGS),
    .NUM_RO_REGS        (NUM_RO_REGS)
  ) ipif_regs_inst
  (   
    .Bus2IP_Clk     ( Bus2IP_Clk     ),
    .Bus2IP_Resetn  ( Bus2IP_Resetn  ), 
    .Bus2IP_Addr    ( Bus2IP_Addr    ),
    .Bus2IP_CS      ( Bus2IP_CS[0]   ),
    .Bus2IP_RNW     ( Bus2IP_RNW     ),
    .Bus2IP_Data    ( Bus2IP_Data    ),
    .Bus2IP_BE      ( Bus2IP_BE      ),
    .IP2Bus_Data    ( IP2Bus_Data0    ),
    .IP2Bus_RdAck   ( IP2Bus_RdAck0   ),
    .IP2Bus_WrAck   ( IP2Bus_WrAck0   ),
    .IP2Bus_Error   ( IP2Bus_Error0   ),
    .wo_regs        ( wo_regs ),	
    .rw_regs        ( rw_regs ),
    .ro_regs        ( ro_regs )
  );

 ipif_table_regs #
  (
   .C_S_AXI_DATA_WIDTH(C_S_AXI_DATA_WIDTH),          
   .C_S_AXI_ADDR_WIDTH(C_S_AXI_ADDR_WIDTH),   
   .TBL_NUM_COLS(1),
   .TBL_NUM_ROWS(DST_IP_FILTER_TABLE_DEPTH)
  ) destip_interface
 (   
   // -- IPIF ports
    .Bus2IP_Clk     ( Bus2IP_Clk     ),
    .Bus2IP_Resetn  ( Bus2IP_Resetn  ), 
    .Bus2IP_Addr    ( Bus2IP_Addr    ),
    .Bus2IP_CS      ( Bus2IP_CS[1]   ),
    .Bus2IP_RNW     ( Bus2IP_RNW     ),
    .Bus2IP_Data    ( Bus2IP_Data    ),
    .Bus2IP_BE      ( Bus2IP_BE      ),
    .IP2Bus_Data    ( IP2Bus_Data1    ),
    .IP2Bus_RdAck   ( IP2Bus_RdAck1   ),
    .IP2Bus_WrAck   ( IP2Bus_WrAck1   ),
    .IP2Bus_Error   ( IP2Bus_Error1   ),
   // -- Table ports
    .tbl_rd_req(tbl_rd_req1),       // Request a read
    .tbl_rd_ack(tbl_rd_ack1),       // Pulses hi on ACK
    .tbl_rd_addr(tbl_rd_addr1),      // Address in table to read
    .tbl_rd_data(tbl_rd_data1),      // Value in table
    .tbl_wr_req(tbl_wr_req1),       // Request a write
    .tbl_wr_ack(tbl_wr_ack1),       // Pulses hi on ACK
    .tbl_wr_addr(tbl_wr_addr1),      // Address in table to write
    .tbl_wr_data(tbl_wr_data1)       // Value to write to table
 );

 ipif_table_regs #
  (
   .C_S_AXI_DATA_WIDTH(C_S_AXI_DATA_WIDTH),          
   .C_S_AXI_ADDR_WIDTH(C_S_AXI_ADDR_WIDTH),   
   .TBL_NUM_COLS(4),
   .TBL_NUM_ROWS(ROUTE_TABLE_DEPTH)
  ) lpm_table_interface
 (   
   // -- IPIF ports
    .Bus2IP_Clk     ( Bus2IP_Clk     ),
    .Bus2IP_Resetn  ( Bus2IP_Resetn  ), 
    .Bus2IP_Addr    ( Bus2IP_Addr    ),
    .Bus2IP_CS      ( Bus2IP_CS[2]   ),
    .Bus2IP_RNW     ( Bus2IP_RNW     ),
    .Bus2IP_Data    ( Bus2IP_Data    ),
    .Bus2IP_BE      ( Bus2IP_BE      ),
    .IP2Bus_Data    ( IP2Bus_Data2    ),
    .IP2Bus_RdAck   ( IP2Bus_RdAck2   ),
    .IP2Bus_WrAck   ( IP2Bus_WrAck2   ),
    .IP2Bus_Error   ( IP2Bus_Error2   ),
   // -- Table ports
    .tbl_rd_req(tbl_rd_req2),       // Request a read
    .tbl_rd_ack(tbl_rd_ack2),       // Pulses hi on ACK
    .tbl_rd_addr(tbl_rd_addr2),      // Address in table to read
    .tbl_rd_data(tbl_rd_data2),      // Value in table
    .tbl_wr_req(tbl_wr_req2),       // Request a write
    .tbl_wr_ack(tbl_wr_ack2),       // Pulses hi on ACK
    .tbl_wr_addr(tbl_wr_addr2),      // Address in table to write
    .tbl_wr_data(tbl_wr_data2)       // Value to write to table
 );

 ipif_table_regs #
  (
   .C_S_AXI_DATA_WIDTH(C_S_AXI_DATA_WIDTH),  
   .C_S_AXI_ADDR_WIDTH(C_S_AXI_ADDR_WIDTH),   
   .TBL_NUM_COLS(3),
   .TBL_NUM_ROWS(ARP_TABLE_DEPTH)
  ) arp_table_interface
 (   
   // -- IPIF ports
    .Bus2IP_Clk     ( Bus2IP_Clk     ),
    .Bus2IP_Resetn  ( Bus2IP_Resetn  ), 
    .Bus2IP_Addr    ( Bus2IP_Addr    ),
    .Bus2IP_CS      ( Bus2IP_CS[3]   ),
    .Bus2IP_RNW     ( Bus2IP_RNW     ),
    .Bus2IP_Data    ( Bus2IP_Data    ),
    .Bus2IP_BE      ( Bus2IP_BE      ),
    .IP2Bus_Data    ( IP2Bus_Data3    ),
    .IP2Bus_RdAck   ( IP2Bus_RdAck3   ),
    .IP2Bus_WrAck   ( IP2Bus_WrAck3   ),
    .IP2Bus_Error   ( IP2Bus_Error3   ),
   // -- Table ports
    .tbl_rd_req(tbl_rd_req3),       // Request a read
    .tbl_rd_ack(tbl_rd_ack3),       // Pulses hi on ACK
    .tbl_rd_addr(tbl_rd_addr3),      // Address in table to read
    .tbl_rd_data(tbl_rd_data3),      // Value in table
    .tbl_wr_req(tbl_wr_req3),       // Request a write
    .tbl_wr_ack(tbl_wr_ack3),       // Pulses hi on ACK
    .tbl_wr_addr(tbl_wr_addr3),      // Address in table to write
    .tbl_wr_data(tbl_wr_data3)       // Value to write to table
 );

  always@*
  begin
    IP2Bus_Data = 0;
    case(Bus2IP_CS)
      4'b0001: IP2Bus_Data =IP2Bus_Data0; 
      4'b0010: IP2Bus_Data =IP2Bus_Data1; 
      4'b0100: IP2Bus_Data =IP2Bus_Data2; 
      4'b1000: IP2Bus_Data =IP2Bus_Data3; 
   endcase
  end

  always@*
  begin
    IP2Bus_WrAck = 0;
    case(Bus2IP_CS)
      4'b0001: IP2Bus_WrAck =IP2Bus_WrAck0; 
      4'b0010: IP2Bus_WrAck =IP2Bus_WrAck1; 
      4'b0100: IP2Bus_WrAck =IP2Bus_WrAck2; 
      4'b1000: IP2Bus_WrAck =IP2Bus_WrAck3; 
   endcase
  end

  always@*
  begin
    IP2Bus_RdAck = 0;
    case(Bus2IP_CS)
      4'b0001: IP2Bus_RdAck =IP2Bus_RdAck0; 
      4'b0010: IP2Bus_RdAck =IP2Bus_RdAck1; 
      4'b0100: IP2Bus_RdAck =IP2Bus_RdAck2; 
      4'b1000: IP2Bus_RdAck =IP2Bus_RdAck3; 
   endcase
  end

 always@*
  begin
    IP2Bus_Error = 0;
    case(Bus2IP_CS)
      4'b0001: IP2Bus_Error =IP2Bus_Error0; 
      4'b0010: IP2Bus_Error =IP2Bus_Error1; 
      4'b0100: IP2Bus_Error =IP2Bus_Error2; 
      4'b1000: IP2Bus_Error =IP2Bus_Error3; 
   endcase
  end

  wire [C_S_AXI_DATA_WIDTH-1:0] reset;
  wire [C_S_AXI_DATA_WIDTH-1:0] mac0_low;
  wire [C_S_AXI_DATA_WIDTH-1:0] mac0_high;
  wire [C_S_AXI_DATA_WIDTH-1:0] mac1_low;
  wire [C_S_AXI_DATA_WIDTH-1:0] mac1_high;
  wire [C_S_AXI_DATA_WIDTH-1:0] mac2_low;
  wire [C_S_AXI_DATA_WIDTH-1:0] mac2_high;
  wire [C_S_AXI_DATA_WIDTH-1:0] mac3_low;
  wire [C_S_AXI_DATA_WIDTH-1:0] mac3_high;
  wire [C_S_AXI_DATA_WIDTH-1:0] ip_addr;
  wire [C_S_AXI_DATA_WIDTH-1:0] wrong_mac_count;
  wire [C_S_AXI_DATA_WIDTH-1:0] lpm_miss_count;
  wire [C_S_AXI_DATA_WIDTH-1:0] arp_miss_count;
  wire [C_S_AXI_DATA_WIDTH-1:0] non_ip_count;
  wire [C_S_AXI_DATA_WIDTH-1:0] dropped_count;
  wire [C_S_AXI_DATA_WIDTH-1:0] forwarded_count;
  wire [C_S_AXI_DATA_WIDTH-1:0] dest_hit_count;
  wire [C_S_AXI_DATA_WIDTH-1:0] bad_ttl_count;
  wire [C_S_AXI_DATA_WIDTH-1:0] ver_count;
  wire [C_S_AXI_DATA_WIDTH-1:0] cpu_count;
  wire [C_S_AXI_DATA_WIDTH-1:0] debugA;
  wire [C_S_AXI_DATA_WIDTH-1:0] debugE;
  wire [31:0] debug1, debug2, debug3, debug4, debug5;

   assign ro_regs = {debug5,debug4,debug3,debug2,debug1,debugA,debugE,cpu_count, ver_count, bad_ttl_count, dest_hit_count, forwarded_count, dropped_count, non_ip_count, arp_miss_count, lpm_miss_count, wrong_mac_count};
   //assign {mac3_high, mac3_low, mac2_high, mac2_low, mac1_high, mac1_low, mac0_high, mac0_low} = rw_regs;
   assign mac0_low = rw_regs[C_S_AXI_DATA_WIDTH-1:0];
   assign mac0_high = rw_regs[(2*C_S_AXI_DATA_WIDTH)-1:C_S_AXI_DATA_WIDTH];
   assign mac1_low = rw_regs[(3*C_S_AXI_DATA_WIDTH)-1:2*C_S_AXI_DATA_WIDTH];
   assign mac1_high = rw_regs[(4*C_S_AXI_DATA_WIDTH)-1:3*C_S_AXI_DATA_WIDTH];
   assign mac2_low = rw_regs[(5*C_S_AXI_DATA_WIDTH)-1:4*C_S_AXI_DATA_WIDTH];
   assign mac2_high = rw_regs[(6*C_S_AXI_DATA_WIDTH)-1:5*C_S_AXI_DATA_WIDTH];
   assign mac3_low = rw_regs[(7*C_S_AXI_DATA_WIDTH)-1:6*C_S_AXI_DATA_WIDTH];
   assign mac3_high = rw_regs[(8*C_S_AXI_DATA_WIDTH)-1:7*C_S_AXI_DATA_WIDTH];




   assign reset  = wo_regs;

    // Master Stream Ports (interface to data path)
    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_0;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_0;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_0;
    wire 				M_AXIS_TVALID_0;
    wire  				M_AXIS_TREADY_0;
    wire 				M_AXIS_TLAST_0;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_1;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_1;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_1;
    wire 				M_AXIS_TVALID_1;
    wire  				M_AXIS_TREADY_1;
    wire 				M_AXIS_TLAST_1;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_2;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_2;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_2;
    wire 				M_AXIS_TVALID_2;
    wire  				M_AXIS_TREADY_2;
    wire 				M_AXIS_TLAST_2;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_3;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_3;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_3;
    wire 				M_AXIS_TVALID_3;
    wire  				M_AXIS_TREADY_3;
    wire 				M_AXIS_TLAST_3;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_4;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_4;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_4;
    wire 				M_AXIS_TVALID_4;
    wire  				M_AXIS_TREADY_4;
    wire 				M_AXIS_TLAST_4;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_5;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_5;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_5;
    wire 				M_AXIS_TVALID_5;
    wire  				M_AXIS_TREADY_5;
    wire 				M_AXIS_TLAST_5;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_6;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_6;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_6;
    wire 				M_AXIS_TVALID_6;
    wire  				M_AXIS_TREADY_6;
    wire 				M_AXIS_TLAST_6;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_7;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_7;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_7;
    wire 				M_AXIS_TVALID_7;
    wire  				M_AXIS_TREADY_7;
    wire 				M_AXIS_TLAST_7;
	 
	 
    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_8;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_8;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_8;
    wire 				M_AXIS_TVALID_8;
    wire  				M_AXIS_TREADY_8;
    wire 				M_AXIS_TLAST_8;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_9;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_9;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_9;
    wire 				M_AXIS_TVALID_9;
    wire  				M_AXIS_TREADY_9;
    wire 				M_AXIS_TLAST_9;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_10;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_10;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_10;
    wire 				M_AXIS_TVALID_10;
    wire  				M_AXIS_TREADY_10;
    wire 				M_AXIS_TLAST_10;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_11;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_11;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_11;
    wire 				M_AXIS_TVALID_11;
    wire  				M_AXIS_TREADY_11;
    wire 				M_AXIS_TLAST_11;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_12;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_12;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_12;
    wire 				M_AXIS_TVALID_12;
    wire  				M_AXIS_TREADY_12;
    wire 				M_AXIS_TLAST_12;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_13;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_13;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_13;
    wire 				M_AXIS_TVALID_13;
    wire  				M_AXIS_TREADY_13;
    wire 				M_AXIS_TLAST_13;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_14;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_14;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_14;
    wire 				M_AXIS_TLAST_14;
    wire 				M_AXIS_TVALID_14;
    wire  				M_AXIS_TREADY_14;

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA_15;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB_15;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]	M_AXIS_TUSER_15;
    wire 				M_AXIS_TLAST_15;
    wire 				M_AXIS_TVALID_15;
    wire  				M_AXIS_TREADY_15;

   fallthrough_small_fifo
        #( .WIDTH(C_M_AXIS_DATA_WIDTH+C_M_AXIS_TUSER_WIDTH+C_M_AXIS_DATA_WIDTH/8+1),
           .MAX_DEPTH_BITS(2))
      input_fifo
        (// Outputs
         .dout                           ({M_AXIS_TLAST_0, M_AXIS_TUSER_0, M_AXIS_TSTRB_0, M_AXIS_TDATA_0}),
         .full                           (),
         .nearly_full                    (in_fifo_nearly_full),
         .prog_full                      (),
         .empty                          (in_fifo_empty),
         // Inputs
       .din                            ({S_AXIS_TLAST, S_AXIS_TUSER, S_AXIS_TSTRB, S_AXIS_TDATA}),
         .wr_en                          (S_AXIS_TVALID & S_AXIS_TREADY),
         .rd_en                          (in_fifo_rd_en),
         .reset                          (~AXI_RESETN),
         .clk                            (AXI_ACLK)
	);

   // ------------- Logic ----------------

   assign S_AXIS_TREADY = !in_fifo_nearly_full;

   // packet is from the cpu if it is on an odd numbered port
   assign pkt_is_from_cpu = M_AXIS_TUSER[SRC_PORT_POS+1] ||
			    M_AXIS_TUSER[SRC_PORT_POS+3] ||
			    M_AXIS_TUSER[SRC_PORT_POS+5] ||
			    M_AXIS_TUSER[SRC_PORT_POS+7];
   // Handle output
   assign in_fifo_rd_en = M_AXIS_TREADY_0 && !in_fifo_empty;
   assign M_AXIS_TVALID_0 = !in_fifo_empty;




   wire [15:0] low_ip_addr,low_ip_addr1,low_ip_addr2;
   wire [31:0] partial_checksum1;
   wire [31:0] partial_checksum2;

   wire [31:0] checksum01,checksum02,checksum03,checksum04,
	       checksum11,checksum12,
	       checksum21,checksum22,
	       checksum31_final;

  checksum1 
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    checksum1 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA_9 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_9 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_9 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_9 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_9 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_9 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_0 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_0 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_0 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_0 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_0 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_0 ),
	.checksum01(checksum01),
	.checksum02(checksum02),
	.checksum03(checksum03),
	.checksum04(checksum04)
   );
   

  checksum2 
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    checksum2 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA_10 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_10 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_10 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_10 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_10 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_10 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_9 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_9 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_9 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_9 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_9 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_9 ),
	.checksum01(checksum01),
	.checksum02(checksum02),
	.checksum03(checksum03),
	.checksum04(checksum04),
	.checksum11(checksum11),
	.checksum12(checksum12),
	.low_ip_addr(low_ip_addr1)
   );
  
  checksum2a
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    checksum2a (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA_15 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_15 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_15 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_15 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_15 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_15 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_10 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_10 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_10 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_10 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_10 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_10 ),
	.checksum11(checksum11),
	.checksum12(checksum12),
	.checksum21(checksum21),
	.checksum22(checksum22)
   );
  checksum3 
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    checksum3 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA_11 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_11 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_11 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_11 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_11 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_11 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_15 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_15 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_15 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_15 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_15 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_15 ),
	.checksum11(checksum21),
	.checksum12(checksum22),
	.checksum_final(checksum31_final),
	.low_ip_addr_in(low_ip_addr1),
	.low_ip_addr_out(low_ip_addr2)
   );
   
   wire [15:0] checksum16;

    ip_addr_checksum_delay
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    ip_addr_checksum_delay (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA_1 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_1 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_1 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_1 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_1 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_1 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_11 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_11 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_11 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_11 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_11 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_11 ),
      .reset(reset),
      .cpu_count(cpu_count),
	.checksum_in(checksum31_final),
	.ip_addr_in(low_ip_addr2),
	.checksum_out(checksum16),
	.low_ip_addr(low_ip_addr)
   );

    wire [31:0] destip_addr,destip_addr1,destip_addr2;
    wire [5:0] cpu_hit_array;
    wire [4:0] drop_array;
  
    drop_packets1
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    drop_packets1 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA_12 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_12 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_12 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_12 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_12 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_12 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_1 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_1 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_1 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_1 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_1 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_1 ),
	.reset(reset),
	.mac0_low (mac0_low),
	.mac0_high(mac0_high),
	.mac1_low (mac1_low),
	.mac1_high(mac1_high),
	.mac2_low (mac2_low),
	.mac2_high(mac2_high),
	.mac3_low (mac3_low),
	.mac3_high(mac3_high),
	.low_ip_addr(low_ip_addr),
	.checksum(checksum16),
	.wrong_mac_count(wrong_mac_count),
	.dropped_count(dropped_count),
	.destip_addr(destip_addr1),
	.drop_array(drop_array),
	.debug_checksum_expected(debugE),
	.debug_checksum_actual(debugA),
	.debug1(debug1),
	.debug2(debug2),
	.debug3(debug3),
	.debug4(debug4),
	.debug5(debug5)
   );


    drop_packets2
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    drop_packets2 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA_2 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_2 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_2 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_2 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_2 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_2 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_12 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_12 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_12 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_12 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_12 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_12 ),
	.destip_in(destip_addr1),
	.destip_addr(destip_addr),
	.drop_array(drop_array)
   );

    dest_ip1
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    dest_ip1 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA_8 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_8 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_8 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_8),
      .M_AXIS_TREADY ( M_AXIS_TREADY_8 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_8 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_2 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_2 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_2 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_2 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_2 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_2 ),
	.reset(reset),
	.non_ip_count(non_ip_count),
//	.dropped_count(dropped_count),
	.bad_ttl_count(bad_ttl_count),
	.ver_count(ver_count),
//	.cpu_count(cpu_count),
	.dest_hit_count(dest_hit_count),
        .destip_addr(destip_addr),
        .ip_addr(destip_addr2),
	.cpu_hit_array(cpu_hit_array),
	.tbl_rd_req(tbl_rd_req1),       // Request a read
	.tbl_rd_ack(tbl_rd_ack1),       // Pulses hi on ACK
	.tbl_rd_addr(tbl_rd_addr1),      // Address in table to read
	.tbl_rd_data(tbl_rd_data1),      // Value in table
	.tbl_wr_req(tbl_wr_req1),       // Request a write
	.tbl_wr_ack(tbl_wr_ack1),       // Pulses hi on ACK
	.tbl_wr_addr(tbl_wr_addr1),      // Address in table to write
	.tbl_wr_data(tbl_wr_data1)       // Value to write to table
   );

    dest_ip2
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    dest_ip2 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA_3 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_3 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_3 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_3),
      .M_AXIS_TREADY ( M_AXIS_TREADY_3 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_3 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_8 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_8 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_8 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_8 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_8 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_8 ),
        .destip_addr(destip_addr2),
        .ip_addr(ip_addr),
	.cpu_hit_array(cpu_hit_array)
   );
   wire arp_lookup_out;
   wire [31:0] nh_reg_in, oq_reg_in,lpm1_ip_addr_out;
   wire [31:0] nh_reg_out, oq_reg_out;
   wire lpm_hit1;
   wire [4:0] index_hit1;  

   wire [63:0] lpm_result0, lpm_result1, lpm_result2, lpm_result3, lpm_result4, lpm_result5, lpm_result6, lpm_result7, 
lpm_result8, lpm_result9, lpm_result10, lpm_result11, lpm_result12, lpm_result13, lpm_result14, lpm_result15, 
lpm_result16, lpm_result17, lpm_result18, lpm_result19, lpm_result20, lpm_result21, lpm_result22, lpm_result23, 
lpm_result24, lpm_result25, lpm_result26, lpm_result27, lpm_result28, lpm_result29, lpm_result30, lpm_result31;
 
  lpm1 
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    lpm1 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
     .M_AXIS_TDATA ( M_AXIS_TDATA_13 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_13 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_13 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_13 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_13 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_13 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_3 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_3 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_3 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_3 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_3 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_3 ), 
      .ip_addr(ip_addr),
      .ip_addr_out(lpm1_ip_addr_out),
//       .nh_reg(nh_reg_in),
//	.oq_reg(oq_reg_in),
	.lpm_hit(lpm_hit1),
	.index_hit(index_hit1),
   // -- Table ports
     .tbl_rd_req(tbl_rd_req2),       // Request a read
    .tbl_rd_ack(tbl_rd_ack2),       // Pulses hi on ACK
    .tbl_rd_addr(tbl_rd_addr2),      // Address in table to read
    .tbl_rd_data(tbl_rd_data2),      // Value in table
    .tbl_wr_req(tbl_wr_req2),       // Request a write
    .tbl_wr_ack(tbl_wr_ack2),       // Pulses hi on ACK
    .tbl_wr_addr(tbl_wr_addr2),      // Address in table to write
    .tbl_wr_data(tbl_wr_data2),       // Value to write to table
	.lpm_result0(lpm_result0),
	.lpm_result1(lpm_result1),
	.lpm_result2(lpm_result2),
	.lpm_result3(lpm_result3),
	.lpm_result4(lpm_result4),
	.lpm_result5(lpm_result5),
	.lpm_result6(lpm_result6),
	.lpm_result7(lpm_result7),
	.lpm_result8(lpm_result8),
	.lpm_result9(lpm_result9),
	.lpm_result10(lpm_result10),
	.lpm_result11(lpm_result11),
	.lpm_result12(lpm_result12),
	.lpm_result13(lpm_result13),
	.lpm_result14(lpm_result14),
	.lpm_result15(lpm_result15),
	.lpm_result16(lpm_result16),
	.lpm_result17(lpm_result17),
	.lpm_result18(lpm_result18),
	.lpm_result19(lpm_result19),
	.lpm_result20(lpm_result20),
	.lpm_result21(lpm_result21),
	.lpm_result22(lpm_result22),
	.lpm_result23(lpm_result23),
	.lpm_result24(lpm_result24),
	.lpm_result25(lpm_result25),
	.lpm_result26(lpm_result26),
	.lpm_result27(lpm_result27),
	.lpm_result28(lpm_result28),
	.lpm_result29(lpm_result29),
	.lpm_result30(lpm_result30),
	.lpm_result31(lpm_result31)
    );

  lpm2
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    lpm2 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
     .M_AXIS_TDATA ( M_AXIS_TDATA_4 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_4 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_4 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_4 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_4 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_4 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_13 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_13 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_13 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_13 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_13 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_13 ), 
	.ip_addr_in(lpm1_ip_addr_out),
	.lpm_hit_in(lpm_hit1),
	.index_hit_in(index_hit1),
       .nh_reg(nh_reg_in),
	.oq_reg(oq_reg_in),
	.lpm_hit(lpm_hit),
	.lpm_result0(lpm_result0),
	.lpm_result1(lpm_result1),
	.lpm_result2(lpm_result2),
	.lpm_result3(lpm_result3),
	.lpm_result4(lpm_result4),
	.lpm_result5(lpm_result5),
	.lpm_result6(lpm_result6),
	.lpm_result7(lpm_result7),
	.lpm_result8(lpm_result8),
	.lpm_result9(lpm_result9),
	.lpm_result10(lpm_result10),
	.lpm_result11(lpm_result11),
	.lpm_result12(lpm_result12),
	.lpm_result13(lpm_result13),
	.lpm_result14(lpm_result14),
	.lpm_result15(lpm_result15),
	.lpm_result16(lpm_result16),
	.lpm_result17(lpm_result17),
	.lpm_result18(lpm_result18),
	.lpm_result19(lpm_result19),
	.lpm_result20(lpm_result20),
	.lpm_result21(lpm_result21),
	.lpm_result22(lpm_result22),
	.lpm_result23(lpm_result23),
	.lpm_result24(lpm_result24),
	.lpm_result25(lpm_result25),
	.lpm_result26(lpm_result26),
	.lpm_result27(lpm_result27),
	.lpm_result28(lpm_result28),
	.lpm_result29(lpm_result29),
	.lpm_result30(lpm_result30),
	.lpm_result31(lpm_result31)
    );

  lpm3 
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    lpm3 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
     .M_AXIS_TDATA ( M_AXIS_TDATA_5 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_5 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_5 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_5 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_5 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_5 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_4 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_4 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_4 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_4 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_4 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_4 ), 
//      .ip_addr(ip_addr),
      .lpm_hit(lpm_hit),
      .reset(reset),
      .lpm_miss_count(lpm_miss_count),
      .nh_reg_in(nh_reg_in),
      .oq_reg_in(oq_reg_in),
   // -- Table ports
/*
     .tbl_rd_req(tbl_rd_req2),       // Request a read
    .tbl_rd_ack(tbl_rd_ack2),       // Pulses hi on ACK
    .tbl_rd_addr(tbl_rd_addr2),      // Address in table to read
    .tbl_rd_data(tbl_rd_data2),      // Value in table
    .tbl_wr_req(tbl_wr_req2),       // Request a write
    .tbl_wr_ack(tbl_wr_ack2),       // Pulses hi on ACK
    .tbl_wr_addr(tbl_wr_addr2),      // Address in table to write
    .tbl_wr_data(tbl_wr_data2),       // Value to write to table
*/
    .arp_lookup(arp_lookup_out),
    .nh_reg(nh_reg_out),
    .oq_reg(oq_reg_out)
    );

	wire arp_hit,arp_hit1;
	wire [31:0] arp_oq_reg,arp_oq_reg1,arp_oq_reg2;
	wire [47:0] dest_mac;
	wire [63:0] dest_mac_table6, dest_mac_table0, dest_mac_table1, dest_mac_table2, dest_mac_table3, dest_mac_table4, dest_mac_table5, dest_mac_table7, dest_mac_table8, dest_mac_table9, dest_mac_table10, dest_mac_table11, dest_mac_table12, dest_mac_table13, dest_mac_table14, dest_mac_table15, dest_mac_table16, dest_mac_table17, dest_mac_table18, dest_mac_table19,dest_mac_table20, dest_mac_table21, dest_mac_table22, dest_mac_table23, dest_mac_table24, dest_mac_table25, dest_mac_table26, dest_mac_table27, dest_mac_table28, dest_mac_table29, dest_mac_table30, dest_mac_table31;
	wire [4:0] index_hit;

    arp1 
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    arp1 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA_6 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_6 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_6 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_6 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_6 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_6 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_5 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_5 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_5 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_5 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_5 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_5 ),
	.arp_lookup(arp_lookup_out),
	.nh_reg(nh_reg_out),
	.oq_reg(oq_reg_out),
	.oq_reg_out(arp_oq_reg1),
	//.dest_mac(dest_mac),
	.arp_hit(arp_hit1),
	.dest_mac_table0(dest_mac_table0),
	.dest_mac_table1(dest_mac_table1),
	.dest_mac_table2(dest_mac_table2),
	.dest_mac_table3(dest_mac_table3),
	.dest_mac_table4(dest_mac_table4),
	.dest_mac_table5(dest_mac_table5),
	.dest_mac_table6(dest_mac_table6),
	.dest_mac_table7(dest_mac_table7),
	.dest_mac_table8(dest_mac_table8),
	.dest_mac_table9(dest_mac_table9),
	.dest_mac_table10(dest_mac_table10),
	.dest_mac_table11(dest_mac_table11),
	.dest_mac_table12(dest_mac_table12),
	.dest_mac_table13(dest_mac_table13),
	.dest_mac_table14(dest_mac_table14),
	.dest_mac_table15(dest_mac_table15),
	.dest_mac_table16(dest_mac_table16),
	.dest_mac_table17(dest_mac_table17),
	.dest_mac_table18(dest_mac_table18),
	.dest_mac_table19(dest_mac_table19),
	.dest_mac_table20(dest_mac_table20),
	.dest_mac_table21(dest_mac_table21),
	.dest_mac_table22(dest_mac_table22),
	.dest_mac_table23(dest_mac_table23),
	.dest_mac_table24(dest_mac_table24),
	.dest_mac_table25(dest_mac_table25),
	.dest_mac_table26(dest_mac_table26),
	.dest_mac_table27(dest_mac_table27),
	.dest_mac_table28(dest_mac_table28),
	.dest_mac_table29(dest_mac_table29),
	.dest_mac_table30(dest_mac_table30),
	.dest_mac_table31(dest_mac_table31),
	.index_hit(index_hit),
   // -- Table ports
    .tbl_rd_req(tbl_rd_req3),       // Request a read
    .tbl_rd_ack(tbl_rd_ack3),       // Pulses hi on ACK
    .tbl_rd_addr(tbl_rd_addr3),      // Address in table to read
    .tbl_rd_data(tbl_rd_data3),      // Value in table
    .tbl_wr_req(tbl_wr_req3),       // Request a write
    .tbl_wr_ack(tbl_wr_ack3),       // Pulses hi on ACK
    .tbl_wr_addr(tbl_wr_addr3),      // Address in table to write
    .tbl_wr_data(tbl_wr_data3)       // Value to write to table
    );

    arp2
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    arp2 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA_7 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_7 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_7 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_7 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_7 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_7 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_6 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_6 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_6 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_6 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_6 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_6 ),
	.arp_lookup(arp_hit1),
//	.nh_reg(nh_reg_out),
	.oq_reg(arp_oq_reg1),
	.oq_reg_out(arp_oq_reg2),
	.dest_mac(dest_mac),
	.arp_hit(arp_hit),
	.dest_mac_table0(dest_mac_table0),
	.dest_mac_table1(dest_mac_table1),
	.dest_mac_table2(dest_mac_table2),
	.dest_mac_table3(dest_mac_table3),
	.dest_mac_table4(dest_mac_table4),
	.dest_mac_table5(dest_mac_table5),
	.dest_mac_table6(dest_mac_table6),
	.dest_mac_table7(dest_mac_table7),
	.dest_mac_table8(dest_mac_table8),
	.dest_mac_table9(dest_mac_table9),
	.dest_mac_table10(dest_mac_table10),
	.dest_mac_table11(dest_mac_table11),
	.dest_mac_table12(dest_mac_table12),
	.dest_mac_table13(dest_mac_table13),
	.dest_mac_table14(dest_mac_table14),
	.dest_mac_table15(dest_mac_table15),
	.dest_mac_table16(dest_mac_table16),
	.dest_mac_table17(dest_mac_table17),
	.dest_mac_table18(dest_mac_table18),
	.dest_mac_table19(dest_mac_table19),
	.dest_mac_table20(dest_mac_table20),
	.dest_mac_table21(dest_mac_table21),
	.dest_mac_table22(dest_mac_table22),
	.dest_mac_table23(dest_mac_table23),
	.dest_mac_table24(dest_mac_table24),
	.dest_mac_table25(dest_mac_table25),
	.dest_mac_table26(dest_mac_table26),
	.dest_mac_table27(dest_mac_table27),
	.dest_mac_table28(dest_mac_table28),
	.dest_mac_table29(dest_mac_table29),
	.dest_mac_table30(dest_mac_table30),
	.dest_mac_table31(dest_mac_table31),
	.index_hit(index_hit)
    );

    arp3
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    arp3 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA_14 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_14 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_14 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_14 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_14 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_14 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_7 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_7 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_7 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_7 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_7 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_7 ),
      .reset(reset),
      .arp_miss_count(arp_miss_count),
//      .arp_lookup(arp_lookup_out),
//    .nh_reg(nh_reg_out),
	.mac0_low (mac0_low),
	.mac0_high(mac0_high),
	.mac1_low (mac1_low),
	.mac1_high(mac1_high),
	.mac2_low (mac2_low),
	.mac2_high(mac2_high),
	.mac3_low (mac3_low),
	.mac3_high(mac3_high),
	.forwarded_count(forwarded_count),
	.oq_reg(arp_oq_reg2),
	.arp_hit(arp_hit),
	.dest_mac(dest_mac)
   // -- Table ports
   );


    arp4
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    arp4 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB ),
      .M_AXIS_TUSER ( M_AXIS_TUSER ),
      .M_AXIS_TVALID ( M_AXIS_TVALID ),
      .M_AXIS_TREADY ( M_AXIS_TREADY ),
      .M_AXIS_TLAST ( M_AXIS_TLAST ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_14 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_14 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_14 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_14 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_14 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_14 )
   );



   // modify the dst port in tuser
/*   always @(*) begin
//      M_AXIS_TUSER = tuser_fifo;
      state_next      = state;

      case(state)
	MODULE_HEADER: begin
	   if (M_AXIS_TVALID) begin

  */
             // Send all packets to ethernet port 1 (nf1)
//		M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b100;
/*		if(!pkt_is_from_cpu)
		begin
                  if(M_AXIS_TUSER[SRC_PORT_POS]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] =   8'b100;
                  if(M_AXIS_TUSER[SRC_PORT_POS+2]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b1;
                  if(M_AXIS_TUSER[SRC_PORT_POS+4]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b1000000;
                  if(M_AXIS_TUSER[SRC_PORT_POS+6]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b10000;
                end
		else //M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b100;
*/              
		 /* Here's how we'd implement a NIC: */

/*		
		 if(pkt_is_from_cpu)
		begin
                 if(M_AXIS_TUSER[SRC_PORT_POS+1]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00000010;
                  if(M_AXIS_TUSER[SRC_PORT_POS+3]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00001000;
                  if(M_AXIS_TUSER[SRC_PORT_POS+5]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00100000;
                  if(M_AXIS_TUSER[SRC_PORT_POS+7]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b10000000;
// 		 $display("T User Value - Src %x Dest %x\n",M_AXIS_TUSER[SRC_PORT_POS+7:SRC_PORT_POS],M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS]);
		end
		else 
		begin
                 if(M_AXIS_TUSER[SRC_PORT_POS+1]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00000010;
                  if(M_AXIS_TUSER[SRC_PORT_POS+3]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00001000;
                  if(M_AXIS_TUSER[SRC_PORT_POS+5]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00100000;
                  if(M_AXIS_TUSER[SRC_PORT_POS+7]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b10000000;

// 		 $display("T User Value - Src %x Dest %x\n",M_AXIS_TUSER[SRC_PORT_POS+7:SRC_PORT_POS],M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS]);
		end		
*/
 
               /*
	       if(pkt_is_from_cpu)
	           M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = {1'b0,
			tuser_fifo[SRC_PORT_POS+7:SRC_PORT_POS+1]};
	       else
		   M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = {
			tuser_fifo[SRC_PORT_POS+6:SRC_PORT_POS], 1'b0};
	       */
/*	       if(M_AXIS_TREADY)
			    state_next = IN_PACKET;
	   end
	end // case: MODULE_HEADER

	IN_PACKET: begin
	   if(M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY) begin
	      state_next = MODULE_HEADER;
	   end
	end
      endcase // case (state)
   end // always @ (*)
*/
/*
 reg c_state;

  always@(posedge AXI_ACLK)
  begin
      if(~AXI_RESETN) begin
	 c_state <= 0;
         ipv4_count <= 0;
         arp_count <= 0;
         ospf_count <= 0;
      end
      else if(!c_state & M_AXIS_TVALID & M_AXIS_TREADY) begin
	 c_state <= 1;
         if(M_AXIS_TDATA[159:144] == 16'h0806) arp_count <= arp_count + 1;
	 else if(M_AXIS_TDATA[159:144] == 16'h0800) begin 
           if(M_AXIS_TDATA[143:140] == 4'd4) ipv4_count <= ipv4_count + 1;
	   if(M_AXIS_TDATA[71:64] == 8'd89) ospf_count <= ospf_count + 1;
         end
      end
      else if(c_state & M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY) c_state <= 0;
  end
*/
/*
 reg [1:0] crc_state;
 reg [191:0] crc_data;
 reg [15:0] checksum;
 reg [31:0] temp;
 reg [15:0] temp1;
 reg [15:0] temp2;
 reg [31:0] temp3;
 integer i;

  always@(posedge AXI_ACLK)
  begin
     if(~AXI_RESETN) begin
	crc_state <= 0;
        checksum <= 0;
	temp <= 0;
     end
     else if(crc_state == 2'd0 & M_AXIS_TREADY) begin
			crc_state <= 1;
			temp <= M_AXIS_TDATA[143:128] + M_AXIS_TDATA[127:112] + M_AXIS_TDATA[111:96] 
			        + M_AXIS_TDATA[95:80] + M_AXIS_TDATA[79:64] + M_AXIS_TDATA[47:32]  
			        + M_AXIS_TDATA[31:16] + M_AXIS_TDATA[15:0];
	  end
	  else if(crc_state == 2'd1)
	  begin
			crc_state <= 2;
			temp2 = M_AXIS_TDATA[255:240];
			temp3 = temp + M_AXIS_TDATA[255:240];
			temp1 = temp3[15:0] + temp3[19:16];
			checksum <= ~temp1;
	  end
     else if(crc_state == 2'd2 & M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY) begin 
         crc_state <= 0;
     end
  end


   always@(posedge AXI_ACLK) 
   begin
      if(~AXI_RESETN) begin
	 state <= MODULE_HEADER;
      end
      else begin
	 state <= state_next;
      end
   end
*/

endmodule // output_port_lookup

/*
    arp 
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    arp (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB ),
      .M_AXIS_TUSER ( M_AXIS_TUSER ),
      .M_AXIS_TVALID ( M_AXIS_TVALID ),
      .M_AXIS_TREADY ( M_AXIS_TREADY ),
      .M_AXIS_TLAST ( M_AXIS_TLAST ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_4 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_4 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_4 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_4 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_4 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_4 ),
      .reset(reset),
      .arp_miss_count(arp_miss_count),
      .arp_lookup(arp_lookup_out),
    .nh_reg(nh_reg_out),
	.mac0_low (mac0_low),
	.mac0_high(mac0_high),
	.mac1_low (mac1_low),
	.mac1_high(mac1_high),
	.mac2_low (mac2_low),
	.mac2_high(mac2_high),
	.mac3_low (mac3_low),
	.mac3_high(mac3_high),
	.forwarded_count(forwarded_count),
    .oq_reg(oq_reg_out),
   // -- Table ports
    .tbl_rd_req(tbl_rd_req3),       // Request a read
    .tbl_rd_ack(tbl_rd_ack3),       // Pulses hi on ACK
    .tbl_rd_addr(tbl_rd_addr3),      // Address in table to read
    .tbl_rd_data(tbl_rd_data3),      // Value in table
    .tbl_wr_req(tbl_wr_req3),       // Request a write
    .tbl_wr_ack(tbl_wr_ack3),       // Pulses hi on ACK
    .tbl_wr_addr(tbl_wr_addr3),      // Address in table to write
    .tbl_wr_data(tbl_wr_data3)       // Value to write to table
    );
  lpm 
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    lpm (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
     .M_AXIS_TDATA ( M_AXIS_TDATA_4 ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB_4 ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_4 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID_4 ),
      .M_AXIS_TREADY ( M_AXIS_TREADY_4 ),
      .M_AXIS_TLAST ( M_AXIS_TLAST_4 ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_3 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_3 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_3 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_3 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_3 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_3 ), 
      .ip_addr(ip_addr),
      .reset(reset),
      .lpm_miss_count(lpm_miss_count),
   // -- Table ports
     .tbl_rd_req(tbl_rd_req2),       // Request a read
    .tbl_rd_ack(tbl_rd_ack2),       // Pulses hi on ACK
    .tbl_rd_addr(tbl_rd_addr2),      // Address in table to read
    .tbl_rd_data(tbl_rd_data2),      // Value in table
    .tbl_wr_req(tbl_wr_req2),       // Request a write
    .tbl_wr_ack(tbl_wr_ack2),       // Pulses hi on ACK
    .tbl_wr_addr(tbl_wr_addr2),      // Address in table to write
    .tbl_wr_data(tbl_wr_data2),       // Value to write to table
    .arp_lookup(arp_lookup),
    .nh_reg(nh_reg),
    .oq_reg(oq_reg)
    );
*/

/*

  packet_classification
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    nf10_pc_0 (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB ),
      .M_AXIS_TUSER ( M_AXIS_TUSER_0 ),
      .M_AXIS_TVALID ( M_AXIS_TVALID ),
      .M_AXIS_TREADY ( M_AXIS_TREADY ),
      .M_AXIS_TLAST ( M_AXIS_TLAST ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_0 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_0 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_0 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_0 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_0 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_0 ),
      .ipv4_count(ipv4_count),
      .arp_count(arp_count),
      .ospf_count(ospf_count)
    );

   lpm 
    #(
      .C_S_AXI_DATA_WIDTH ( 32 ),
      .C_S_AXI_ADDR_WIDTH ( 32 ),
      .C_S_AXI_ACLK_FREQ_HZ ( 160000000 ),
      .C_M_AXIS_DATA_WIDTH ( 256 ),
      .C_S_AXIS_DATA_WIDTH ( 256 ),
      .C_M_AXIS_TUSER_WIDTH ( 128 ),
      .C_S_AXIS_TUSER_WIDTH ( 128 )
    )
    lpm (
      .AXI_ACLK ( AXI_ACLK ),
      .AXI_RESETN ( AXI_RESETN ),
      .M_AXIS_TDATA ( M_AXIS_TDATA ),
      .M_AXIS_TSTRB ( M_AXIS_TSTRB ),
      .M_AXIS_TUSER ( tuser_fifo ),
      .M_AXIS_TVALID ( M_AXIS_TVALID ),
      .M_AXIS_TREADY ( M_AXIS_TREADY ),
      .M_AXIS_TLAST ( M_AXIS_TLAST ),
      .S_AXIS_TDATA ( M_AXIS_TDATA_1 ),
      .S_AXIS_TSTRB ( M_AXIS_TSTRB_1 ),
      .S_AXIS_TUSER ( M_AXIS_TUSER_1 ),
      .S_AXIS_TVALID ( M_AXIS_TVALID_1 ),
      .S_AXIS_TREADY ( M_AXIS_TREADY_1 ),
      .S_AXIS_TLAST ( M_AXIS_TLAST_1 ),
   // -- Table ports
    .tbl_rd_req(tbl_rd_req0),       // Request a read
    .tbl_rd_ack(tbl_rd_ack0),       // Pulses hi on ACK
    .tbl_rd_addr(tbl_rd_addr0),      // Address in table to read
    .tbl_rd_data(tbl_rd_data0),      // Value in table
    .tbl_wr_req(tbl_wr_req0),       // Request a write
    .tbl_wr_ack(tbl_wr_ack0),       // Pulses hi on ACK
    .tbl_wr_addr(tbl_wr_addr0),      // Address in table to write
    .tbl_wr_data(tbl_wr_data0)       // Value to write to table
    );
*/

