

module lpm 
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
    output reg[((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB,
    output reg[C_M_AXIS_TUSER_WIDTH-1:0]M_AXIS_TUSER,
    output reg				M_AXIS_TVALID,
    input  				M_AXIS_TREADY,
    output reg				M_AXIS_TLAST,

    // Slave Stream Ports (interface to RX queues)
    input [C_S_AXIS_DATA_WIDTH-1:0] 	S_AXIS_TDATA,
    input [((C_S_AXIS_DATA_WIDTH/8))-1:0]S_AXIS_TSTRB,
    input [C_S_AXIS_TUSER_WIDTH-1:0] 	S_AXIS_TUSER,
    input  				S_AXIS_TVALID,
    output 				S_AXIS_TREADY,
    input  				S_AXIS_TLAST,
    input [C_S_AXI_DATA_WIDTH-1:0]	ip_addr,
    input [C_S_AXI_DATA_WIDTH-1:0]	reset,
    output reg [C_S_AXI_DATA_WIDTH-1:0] lpm_miss_count,
    input tbl_rd_req,       // Request a read
    input tbl_wr_req,       // Request a write
    input 	[4:0] tbl_rd_addr,      // Address in table to read
    input	[4:0] tbl_wr_addr,      // Address in table to write
    input	[C_S_AXI_DATA_WIDTH*4-1:0] tbl_wr_data,      // Value to write to table
    output reg 	[C_S_AXI_DATA_WIDTH*4-1:0] tbl_rd_data,      // Value in table
    output reg tbl_wr_ack,       // Pulses hi on ACK
    output reg tbl_rd_ack,      // Pulses hi on ACK

    output reg [C_S_AXI_DATA_WIDTH-1:0] arp_miss_count,
    input tbl_rd_req0,       // Request a read
    input tbl_wr_req0,       // Request a write
    input 	[4:0] tbl_rd_addr0,      // Address in table to read
    input 	[4:0] tbl_wr_addr0,      // Address in table to write
    input 	[C_S_AXI_DATA_WIDTH*3-1:0] tbl_wr_data0,      // Value to write to table
    output reg 	[C_S_AXI_DATA_WIDTH*3-1:0] tbl_rd_data0,      // Value in table
    output reg tbl_wr_ack0,       // Pulses hi on ACK
    output reg tbl_rd_ack0      // Pulses hi on ACK
/*
    output reg [C_S_AXI_DATA_WIDTH-1:0] ipv4_count,
    output reg [C_S_AXI_DATA_WIDTH-1:0] arp_count,
    output reg [C_S_AXI_DATA_WIDTH-1:0] ospf_count
 */
);

    reg	[C_S_AXI_DATA_WIDTH*3-1:0] arp_table [31:0];      // Value in table
    reg	[C_S_AXI_DATA_WIDTH*4-1:0] lpm_table [31:0];      // Value in table


  always@(posedge AXI_ACLK)
  begin
    if(tbl_wr_req0)
    begin
      tbl_wr_ack0 <= 1;
      arp_table[tbl_wr_addr0] <= tbl_wr_data0;
    end
    else tbl_wr_ack0 <= 0; 
  end


  always@(posedge AXI_ACLK)
  begin
    if(tbl_rd_req0)
    begin
      tbl_rd_ack0 <= 1;
      tbl_rd_data0 <= arp_table[tbl_rd_addr0];
    end
    else tbl_rd_ack0 <= 0; 
  end


  always@(posedge AXI_ACLK)
  begin
    if(tbl_wr_req)
    begin
      tbl_wr_ack <= 1;
      lpm_table[tbl_wr_addr] <= tbl_wr_data;
    end
    else tbl_wr_ack <= 0; 
  end


  always@(posedge AXI_ACLK)
  begin
    if(tbl_rd_req)
    begin
      tbl_rd_ack <= 1;
      tbl_rd_data <= lpm_table[tbl_rd_addr];
    end
    else tbl_rd_ack <= 0; 
  end

  wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA0;
  wire [((C_M_AXIS_DATA_WIDTH/8))-1:0] M_AXIS_TSTRB0;
  wire [C_M_AXIS_TUSER_WIDTH-1:0]      M_AXIS_TUSER0;
  wire 				M_AXIS_TVALID0;
  wire 				M_AXIS_TLAST0;
 
  always@(posedge AXI_ACLK)
  begin
    M_AXIS_TDATA1  <= M_AXIS_TDATA0;
    M_AXIS_TSTRB1  <= M_AXIS_TSTRB0;
//     M_AXIS_TUSER1  <= M_AXIS_TUSER0;
    M_AXIS_TVALID1 <= M_AXIS_TVALID0;
    M_AXIS_TLAST1  <= M_AXIS_TLAST0;
  end


  reg [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA1;
  reg [((C_M_AXIS_DATA_WIDTH/8))-1:0] M_AXIS_TSTRB1;
  reg [C_M_AXIS_TUSER_WIDTH-1:0]      M_AXIS_TUSER1;
  reg 				M_AXIS_TVALID1;
  reg 				M_AXIS_TLAST1;
 
  always@(posedge AXI_ACLK)
  begin
//    M_AXIS_TDATA  <= M_AXIS_TDATA1;
    M_AXIS_TSTRB  <= M_AXIS_TSTRB1;
//     M_AXIS_TUSER1  <= M_AXIS_TUSER0;
    M_AXIS_TVALID <= M_AXIS_TVALID1;
    M_AXIS_TLAST  <= M_AXIS_TLAST1;
  end
   fallthrough_small_fifo
        #( .WIDTH(C_M_AXIS_DATA_WIDTH+C_M_AXIS_TUSER_WIDTH+C_M_AXIS_DATA_WIDTH/8+1),
           .MAX_DEPTH_BITS(2))
      input_fifo
        (// Outputs
         .dout                           ({M_AXIS_TLAST0, M_AXIS_TUSER0, M_AXIS_TSTRB0, M_AXIS_TDATA0}),
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
   assign M_AXIS_TVALID0 = !in_fifo_empty;
   assign S_AXIS_TREADY = !in_fifo_nearly_full;

   reg lpm_hit,arp_lookup,arp_hit;
   reg [1:0] state;
   reg [31:0] ip_check,ip_temp,mask_temp,queue;
	reg [47:0] dest_mac;
   reg [31:0] ip_mask, net_mask, next_hop, oq, nh_reg, oq_reg;
   integer i;

   always@(posedge AXI_ACLK)
   begin
     M_AXIS_TUSER1  = M_AXIS_TUSER0;
     M_AXIS_TUSER   = M_AXIS_TUSER1;
	  M_AXIS_TDATA   = M_AXIS_TDATA1;
      if(~AXI_RESETN)
      begin
        state <= 0;
        lpm_miss_count <= 0;
	nh_reg <= 0;
	oq_reg <= 0;
      end
      else if(reset)
      begin
        lpm_miss_count <= 0;
      end
     else if(state == 0 & M_AXIS_TVALID0 & M_AXIS_TREADY)
     begin
       state <= 2; 
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
          if(M_AXIS_TUSER0[SRC_PORT_POS])   M_AXIS_TUSER1[DST_PORT_POS+7:DST_PORT_POS] =   8'b00000010;
          if(M_AXIS_TUSER0[SRC_PORT_POS+2]) M_AXIS_TUSER1[DST_PORT_POS+7:DST_PORT_POS] = 8'b00001000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+4]) M_AXIS_TUSER1[DST_PORT_POS+7:DST_PORT_POS] = 8'b00100000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+6]) M_AXIS_TUSER1[DST_PORT_POS+7:DST_PORT_POS] = 8'b10000000;
	end
	else
	begin
 	  oq_reg <= oq;	
	  nh_reg <= next_hop;
	  arp_lookup <= lpm_hit;
	end

       end
       end
       else if(	state == 2 & M_AXIS_TREADY & M_AXIS_TVALID1) 
       begin	
	state <= 3;	 
	if(arp_lookup)
	begin
	 dest_mac = 0;
	 queue = 0;
	 arp_hit = 0; 
	 for(i=0;i<32;i=i+1)
	 begin
	     if( !arp_hit & nh_reg == arp_table[i][31:0])
	     begin
		dest_mac = arp_table[i][79:32];
		arp_hit = 1;
	     end
	 end

	if(arp_hit)
	begin
	  case(oq_reg)	
          0: M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] =   8'b00000001;
          1: M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00000100;
          2: M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00010000;
          3: M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b01000000;
          4: M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00000010;	 
			 endcase
	  M_AXIS_TDATA[255:208] = dest_mac;
	  M_AXIS_TDATA[79:72] = M_AXIS_TDATA[79:72] - 1;
	  M_AXIS_TDATA[63:48] = M_AXIS_TDATA[63:48] + 1;
	end
	else 
	begin
	  arp_miss_count <= arp_miss_count + 1;
          if(M_AXIS_TUSER1[SRC_PORT_POS])   M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] =   8'b00000010;
          if(M_AXIS_TUSER1[SRC_PORT_POS+2]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00001000;
          if(M_AXIS_TUSER1[SRC_PORT_POS+4]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00100000;
          if(M_AXIS_TUSER1[SRC_PORT_POS+6]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b10000000;

	end
	end
       end
       else if( state == 3 & M_AXIS_TLAST1 & M_AXIS_TREADY & M_AXIS_TVALID1)
       begin
	state <= 0;
       end

   end




endmodule

