

module arp1
#(
    parameter C_S_AXI_DATA_WIDTH = 32,
    parameter C_S_AXI_ADDR_WIDTH = 32,
    parameter C_USE_WSTRB = 0,
    parameter C_DPHASE_TIMEOUT = 0,
    parameter C_S_AXI_ACLK_FREQ_HZ = 100,
    //Master AXI Stream Data Width
    parameter C_M_AXIS_DATA_WIDTH=256,
    parameter C_S_AXIS_DATA_WIDTH=256,
    parameter C_M_AXIS_TUSER_WIDTH=128,
    parameter C_S_AXIS_TUSER_WIDTH=128,
    parameter SRC_PORT_POS=16,
    parameter DST_PORT_POS=24
)
(
    // Global Ports
    input 				AXI_ACLK,
    input 				AXI_RESETN,

    // Master Stream Ports (interface to data path)
    output reg[C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA,
    output [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB,
    output reg[C_M_AXIS_TUSER_WIDTH-1:0]M_AXIS_TUSER,
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
    input tbl_rd_req,       // Request a read
    input tbl_wr_req,       // Request a write
    input 	[4:0] tbl_rd_addr,      // Address in table to read
    input 	[4:0] tbl_wr_addr,      // Address in table to write
    input 	[C_S_AXI_DATA_WIDTH*3-1:0] tbl_wr_data,      // Value to write to table
    output reg 	[C_S_AXI_DATA_WIDTH*3-1:0] tbl_rd_data,      // Value in table
    output reg tbl_wr_ack,       // Pulses hi on ACK
    output reg tbl_rd_ack,      // Pulses hi on ACK
    input arp_lookup,
    input [31:0] nh_reg,
    input [31:0] oq_reg,
//    output reg [47:0] dest_mac,
    output reg arp_hit, 
    output reg [31:0] oq_reg_out,
    output reg [4:0] index_hit,
    output [63:0] dest_mac_table6, dest_mac_table0, dest_mac_table1, dest_mac_table2, dest_mac_table3, dest_mac_table4, dest_mac_table5, dest_mac_table7, dest_mac_table8, dest_mac_table9,
dest_mac_table10, dest_mac_table11, dest_mac_table12, dest_mac_table13, dest_mac_table14, dest_mac_table15, dest_mac_table16, dest_mac_table17, dest_mac_table18, dest_mac_table19,
dest_mac_table20, dest_mac_table21, dest_mac_table22, dest_mac_table23, dest_mac_table24, dest_mac_table25, dest_mac_table26, dest_mac_table27, dest_mac_table28, dest_mac_table29, dest_mac_table30, dest_mac_table31
);


    reg	[C_S_AXI_DATA_WIDTH*3-1:0] arp_table [0:31];      // Value in table

   integer i,j;

/*
   genvar x;
   generate 
   for(x=0; x < 32; x=x+1)
   begin: dest_mac

	assign dest_mac_table[x] = arp_table[x][95:32];

   end
   endgenerate
*/

  assign dest_mac_table0 = arp_table[0][95:32];
  assign dest_mac_table1 = arp_table[1][95:32];
  assign dest_mac_table2 = arp_table[2][95:32];
  assign dest_mac_table3 = arp_table[3][95:32];
  assign dest_mac_table4 = arp_table[4][95:32];
  assign dest_mac_table5 = arp_table[5][95:32];
  assign dest_mac_table6 = arp_table[6][95:32];
  assign dest_mac_table7 = arp_table[7][95:32];
  assign dest_mac_table8 = arp_table[8][95:32];
  assign dest_mac_table9 = arp_table[9][95:32];
  assign dest_mac_table10 = arp_table[10][95:32];
  assign dest_mac_table11 = arp_table[11][95:32];
  assign dest_mac_table12 = arp_table[12][95:32];
  assign dest_mac_table13 = arp_table[13][95:32];
  assign dest_mac_table14 = arp_table[14][95:32];
  assign dest_mac_table15 = arp_table[15][95:32];
  assign dest_mac_table16 = arp_table[16][95:32];
  assign dest_mac_table17 = arp_table[17][95:32];
  assign dest_mac_table18 = arp_table[18][95:32];
  assign dest_mac_table19 = arp_table[19][95:32];
  assign dest_mac_table20 = arp_table[20][95:32];
  assign dest_mac_table21 = arp_table[21][95:32];
  assign dest_mac_table22 = arp_table[22][95:32];
  assign dest_mac_table23 = arp_table[23][95:32];
  assign dest_mac_table24 = arp_table[24][95:32];
  assign dest_mac_table25 = arp_table[25][95:32];
  assign dest_mac_table26 = arp_table[26][95:32];
  assign dest_mac_table27 = arp_table[27][95:32];
  assign dest_mac_table28 = arp_table[28][95:32];
  assign dest_mac_table29 = arp_table[29][95:32];
  assign dest_mac_table30 = arp_table[30][95:32];
  assign dest_mac_table31 = arp_table[31][95:32];


  always@(posedge AXI_ACLK)
  begin
    if(~AXI_RESETN)
    begin
	for(i=0; i < 32; i=i+1)
	begin
	  arp_table[i] <= 96'hffffffffffffffffffffffff;
	end
    end
    else if(tbl_wr_req)
    begin
      tbl_wr_ack <= 1;
      arp_table[tbl_wr_addr] <= tbl_wr_data;
    end
    else tbl_wr_ack <= 0; 
  end


  always@(posedge AXI_ACLK)
  begin
    if(tbl_rd_req)
    begin
      tbl_rd_ack <= 1;
      tbl_rd_data <= arp_table[tbl_rd_addr];
    end
    else tbl_rd_ack <= 0; 
  end

  wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA0 ;
  reg [C_M_AXIS_DATA_WIDTH-1:0] 	tdata;
  wire [((C_M_AXIS_DATA_WIDTH/8))-1:0] M_AXIS_TSTRB0;
  wire [C_M_AXIS_TUSER_WIDTH-1:0]      M_AXIS_TUSER0;
  wire 				M_AXIS_TVALID0;
  wire 				M_AXIS_TLAST0;

   fallthrough_small_fifo
        #( .WIDTH(C_M_AXIS_DATA_WIDTH+C_M_AXIS_TUSER_WIDTH+C_M_AXIS_DATA_WIDTH/8+1),
           .MAX_DEPTH_BITS(2))
      input_fifo
        (// Outputs
         .dout                           ({M_AXIS_TLAST, M_AXIS_TUSER0, M_AXIS_TSTRB, M_AXIS_TDATA0}),
         .full                           (),
         .nearly_full                    (in_fifo_nearly_full),
         .prog_full                      (),
         .empty                          (in_fifo_empty),
         // Inputs
         .din                            ({S_AXIS_TLAST, S_AXIS_TUSER, S_AXIS_TSTRB, S_AXIS_TDATA}),
         .wr_en                          (S_AXIS_TVALID & S_AXIS_TREADY),
         .rd_en                          (in_fifo_rd_en),
         .reset                          (~AXI_RESETN),
         .clk                            (AXI_ACLK));

   assign in_fifo_rd_en = M_AXIS_TREADY	&& !in_fifo_empty;
   assign M_AXIS_TVALID = !in_fifo_empty;
   assign S_AXIS_TREADY = !in_fifo_nearly_full;


  reg [1:0] state, state_next;
   reg arp_hit_next;
   reg [31:0] arp_miss_next, forwarded_next, ip_check,ip_temp,mask_temp,queue,oq_next;
	reg [47:0] dmac_next;
   reg [31:0] ip_mask, net_mask, next_hop, oq, nh_compare;
   reg [95:0] table_line;
   reg result [0:31];
   reg [4:0] index_hit_next;
   reg [31:0] result_final;

   always@(arp_table[0],arp_table[1],arp_table[2],arp_table[3],arp_table[4],arp_table[5],
arp_table[6],arp_table[7],arp_table[8],arp_table[9],arp_table[10],arp_table[11],arp_table[12],
arp_table[13],arp_table[14],arp_table[15],arp_table[16],arp_table[17],arp_table[18],arp_table[19],
arp_table[20],arp_table[21],arp_table[22],arp_table[23],arp_table[24],arp_table[26],arp_table[27],
arp_table[28],arp_table[29],arp_table[30],arp_table[31],M_AXIS_TDATA0,M_AXIS_TUSER0,
state,M_AXIS_TLAST,M_AXIS_TVALID,M_AXIS_TREADY,nh_reg,arp_lookup,oq_reg,oq_reg_out )
   begin
     M_AXIS_TUSER   = M_AXIS_TUSER0;
     M_AXIS_TDATA   = M_AXIS_TDATA0;
	state_next = state;
	arp_hit_next = arp_hit;
	oq_next = oq_reg_out;
	index_hit_next = index_hit;
       if( (state == 2'd0) & M_AXIS_TVALID & !M_AXIS_TLAST ) 
       begin	
	    state_next = 2'd1;
	if( M_AXIS_TUSER0[DST_PORT_POS+7:DST_PORT_POS] == 8'd0)
	begin

        if(arp_lookup)
	begin
	 for(j=0;j<32;j=j+1)
	 begin
//	table_line = arp_table[j];
//	nh_compare = table_line[31:0];
//	result[j] = 0;
	oq_next= oq_reg;
	     if( nh_reg == arp_table[j][31:0] )
	     begin
		index_hit_next = j;
		arp_hit_next = 1;
	     end
	 end
	end
       end
       end  
       else if( (state == 2'd1) & M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY)
       begin
	state_next = 2'd0;
	arp_hit_next = 0;
       end
   end


  always@(posedge AXI_ACLK)
  begin
      if(~AXI_RESETN)
      begin	
//	oq_reg_out <= oq_reg;
        state <= 0;
//	dest_mac <= 0;
	arp_hit <= 0;
	oq_reg_out <= 0;
	index_hit <= 0;
      end
      else 
      begin
	oq_reg_out <= oq_next;
	state <= state_next;
//	dest_mac <= dmac_next;
	arp_hit <= arp_hit_next;
	index_hit <= index_hit_next;
      end
  end

endmodule
/*
     else if(state == 0 & M_AXIS_TVALID0 & M_AXIS_TREADY)
     begin
       state <= 1; 
       if( !(M_AXIS_TUSER0[DST_PORT_POS+1] || M_AXIS_TUSER0[DST_PORT_POS+3] || M_AXIS_TUSER0[DST_PORT_POS+5] || M_AXIS_TUSER0[DST_PORT_POS+7]) )
       begin
	 ip_check = ip_addr;
	 ip_mask = 0;
	 net_mask = 0;
	 lpm_hit = 0;
	 for(i=0;i<32;i=i+1)
	 begin
	   ip_temp = lpm_table[i][31:0];
	   mask_temp = lpm_table[i][63:32];
	   if(ip_temp||mask_temp > ip_mask) 
	   begin
	     ip_mask = ip_temp; 
	     net_mask = mask_temp;
	     lpm_hit = 1;
	     oq = lpm_table[i][127:96];
	     next_hop = lpm_table[i][95:64];
	   end
	   else if(ip_temp||mask_temp == ip_mask)
	   begin
	     if(mask_temp > net_mask )
	     begin
		ip_mask = ip_temp;
		net_mask = mask_temp;
		lpm_hit = 1;
	   	oq = lpm_table[i][127:96];
	   	next_hop = lpm_table[i][95:64];
	     end
	   end
	 end
	
	if(!lpm_hit)
	begin
	  lpm_miss_count <= lpm_miss_count + 1;
          if(M_AXIS_TUSER0[SRC_PORT_POS])   M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] =   8'b00000010;
          if(M_AXIS_TUSER0[SRC_PORT_POS+2]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00001000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+4]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00100000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+6]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b10000000;
	end
	else
	begin
 	  oq_reg <= oq;	
	  nh_reg <= next_hop;
	  arp_lookup <= lpm_hit;
	end

       end
       end
*/
/*
	  case(oq_reg)	
          0: M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] =   8'b00000001;
          1: M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00000100;
          2: M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00010000;
          3: M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b01000000;
          4: M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00000010;	 
	  endcase
*/
//!( M_AXIS_TUSER0[SRC_PORT_POS] || M_AXIS_TUSER0[SRC_PORT_POS+2] || M_AXIS_TUSER0[SRC_PORT_POS+6] || M_AXIS_TUSER0[SRC_PORT_POS+8]) && (
/*
	result_final = {
	result[0],result[1],result[2],result[3],result[4],result[5],
	result[6],result[7],result[8],result[9],result[10],result[11],
	result[12],result[13],result[14],result[15],result[16],result[17],
	result[18],result[19],result[20],result[21],result[22],result[23],
	result[24],result[25],result[26],result[27],result[28],result[29],
	result[30],result[31]
};

	if(result_final !=32'd0)
	begin
	arp_hit_next = 1;
	case(result_final)
	32'h00000001: index = 31; // 0;
	32'h00000002: index = 30; //1;
	32'h00000004: index = 29;  //2;
	32'h00000008: index = 28; //3;
	32'h00000010: index = 27; //4;
	32'h00000020: index = 26; //5;
	32'h00000040: index = 25; //6;
	32'h00000080: index = 24; //7;
	32'h00000100: index = 23; //8;
	32'h00000200: index = 22; // 9;
	32'h00000400: index = 21; //10;
	32'h00000800: index = 20; //11;
	32'h00001000: index = 19; //12;
	32'h00002000: index = 18; //13;
	32'h00004000: index = 17; //14;
	32'h00008000: index = 16; //15;
	32'h00010000: index = 15; //16;
	32'h00020000: index = 14; //17;
	32'h00040000: index = 13; //18;
	32'h00080000: index = 12; //19;
	32'h00100000: index = 11; //20;
	32'h00200000: index = 10; //21;
	32'h00400000: index = 9; //22;
	32'h00800000: index = 8; //23;
	32'h01000000: index = 7; //24;
	32'h02000000: index = 6; //25;
	32'h04000000: index = 5; //26;
	32'h08000000: index = 4; //27;
	32'h10000000: index = 3; //28;
	32'h20000000: index = 2; //29;
	32'h30000000: index = 1; //30;
	32'h40000000: index = 0; //31;
//	default: index = 0;
	endcase

	dmac_next = arp_table[index][79:32];
	oq_next = oq_reg;
	end
*/
/*
	if(arp_hit)
	begin
	  M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = oq_reg[7:0];
	  M_AXIS_TDATA[255:208] = dest_mac;
	  case(oq_reg[7:0])	
          1: M_AXIS_TDATA[207:160] = {mac0_high[15:0],mac0_low};
          4: M_AXIS_TDATA[207:160] = {mac1_high[15:0],mac1_low};
          16: M_AXIS_TDATA[207:160] = {mac2_high[15:0],mac2_low};
          64: M_AXIS_TDATA[207:160] = {mac3_high[15:0],mac3_low};
	  endcase
	  M_AXIS_TDATA[79:72] = M_AXIS_TDATA0[79:72] - 1;
//	  M_AXIS_TDATA[63:48] = M_AXIS_TDATA0[63:48] + 1;
	  M_AXIS_TDATA[63:56] = M_AXIS_TDATA0[63:56] + 1;
	  forwarded_next = forwarded_next + 1;
	end
	else 
	begin
	  arp_miss_next = arp_miss_next + 1;
          if(M_AXIS_TUSER0[SRC_PORT_POS])   M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] =   8'b00000010;
          if(M_AXIS_TUSER0[SRC_PORT_POS+2]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00001000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+4]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00100000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+6]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b10000000;

	end
	end
*/
