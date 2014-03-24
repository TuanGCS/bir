

module arp3
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
    input [C_S_AXI_DATA_WIDTH-1:0]	reset,
    output reg [C_S_AXI_DATA_WIDTH-1:0] arp_miss_count,
    input [C_S_AXI_DATA_WIDTH-1:0] mac0_low,
    input [C_S_AXI_DATA_WIDTH-1:0] mac0_high,
    input [C_S_AXI_DATA_WIDTH-1:0] mac1_low,
    input [C_S_AXI_DATA_WIDTH-1:0] mac1_high,
    input [C_S_AXI_DATA_WIDTH-1:0] mac2_low,
    input [C_S_AXI_DATA_WIDTH-1:0] mac2_high,
    input [C_S_AXI_DATA_WIDTH-1:0] mac3_low,
    input [C_S_AXI_DATA_WIDTH-1:0] mac3_high,
    output reg [C_S_AXI_DATA_WIDTH-1:0] forwarded_count,
    input [47:0] dest_mac,
    input arp_hit,
    input [31:0] oq_reg
/*
    output reg [C_S_AXI_DATA_WIDTH-1:0] arp_count,
    output reg [C_S_AXI_DATA_WIDTH-1:0] ospf_count
*/
);



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
  reg [31:0] arp_miss_next, forwarded_next, ip_check,ip_temp,mask_temp,queue;

   always@* 
   begin
     M_AXIS_TUSER   = M_AXIS_TUSER0;
     M_AXIS_TDATA   = M_AXIS_TDATA0;
	  state_next = state;
	  arp_miss_next = arp_miss_count;
	  forwarded_next = forwarded_count;
       if( (state == 2'd0) & M_AXIS_TVALID & !M_AXIS_TLAST ) 
       begin//{	
	    state_next = 2'd2;
	if( M_AXIS_TUSER0[DST_PORT_POS+7:DST_PORT_POS] == 8'd0)
	begin//{

	if(arp_hit)
	begin//{
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
	end//}
	else 
	begin//{
	  arp_miss_next = arp_miss_next + 1;
          if(M_AXIS_TUSER0[SRC_PORT_POS])   M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] =   8'b00000010;
          if(M_AXIS_TUSER0[SRC_PORT_POS+2]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00001000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+4]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00100000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+6]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b10000000;
	end//}
	end//}
//	          tdata = M_AXIS_TDATA;
       end//}
       else if( (state == 2'd1) & !M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY)
       begin
	M_AXIS_TDATA = tdata;
	state_next = 2'd2;
       end  
       else if( (state == 2'd2) & M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY)
       begin
	state_next = 2'd0;
       end
   end


  always@(posedge AXI_ACLK)
  begin
	tdata <= M_AXIS_TDATA;
      if(~AXI_RESETN)
      begin
        state <= 0;
	arp_miss_count <= 0;
	forwarded_count <= 0;
      end
      else if(reset == 32'd1)
      begin
	state <= state_next;
        arp_miss_count <= 0;
	forwarded_count <= 0;
      end
      else 
      begin
	state <= state_next;
	arp_miss_count <= arp_miss_next;
	forwarded_count <= forwarded_next;
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
