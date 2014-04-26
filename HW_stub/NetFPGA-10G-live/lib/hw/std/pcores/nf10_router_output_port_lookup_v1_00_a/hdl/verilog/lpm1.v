

module lpm1
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
    output [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA,
    output [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB,
    output [C_M_AXIS_TUSER_WIDTH-1:0]M_AXIS_TUSER,
    output 				M_AXIS_TVALID,
    input  				M_AXIS_TREADY,
    output				M_AXIS_TLAST,

    // Slave Stream Ports (interface to RX queues)
    input [C_S_AXIS_DATA_WIDTH-1:0] 	S_AXIS_TDATA,
    input [((C_S_AXIS_DATA_WIDTH/8))-1:0]S_AXIS_TSTRB,
    input [C_S_AXIS_TUSER_WIDTH-1:0] 	S_AXIS_TUSER,
    input  				S_AXIS_TVALID,
    output 				S_AXIS_TREADY,
    input  				S_AXIS_TLAST,
    input [C_S_AXI_DATA_WIDTH-1:0]	ip_addr,
    output reg [C_S_AXI_DATA_WIDTH-1:0]	ip_addr_out,
//    input [C_S_AXI_DATA_WIDTH-1:0]	reset,
//    output reg [C_S_AXI_DATA_WIDTH-1:0] lpm_miss_count,
    input tbl_rd_req,       // Request a read
    input tbl_wr_req,       // Request a write
    input 	[4:0] tbl_rd_addr,      // Address in table to read
    input	[4:0] tbl_wr_addr,      // Address in table to write
    input	[C_S_AXI_DATA_WIDTH*4-1:0] tbl_wr_data,      // Value to write to table
    output reg 	[C_S_AXI_DATA_WIDTH*4-1:0] tbl_rd_data,      // Value in table
    output reg tbl_wr_ack,       // Pulses hi on ACK
    output reg tbl_rd_ack,      // Pulses hi on ACK
    output reg lpm_hit,
    output reg [4:0] index_hit,
//    output reg arp_lookup,
//    output reg [31:0] nh_reg,
//    output reg [31:0] oq_reg
    output [63:0] lpm_result0, lpm_result1, lpm_result2, lpm_result3, lpm_result4, lpm_result5, lpm_result6, lpm_result7, 
lpm_result8, lpm_result9, lpm_result10, lpm_result11, lpm_result12, lpm_result13, lpm_result14, lpm_result15, 
lpm_result16, lpm_result17, lpm_result18, lpm_result19, lpm_result20, lpm_result21, lpm_result22, lpm_result23, 
lpm_result24, lpm_result25, lpm_result26, lpm_result27, lpm_result28, lpm_result29, lpm_result30, lpm_result31
);

    reg	[C_S_AXI_DATA_WIDTH*4-1:0] lpm_table [0:31];      // Value in table



   integer i,j,k;

  always@(posedge AXI_ACLK)
  begin
    if(~AXI_RESETN)
    begin
	for(i=0; i < 32; i=i+1) lpm_table[i] <= 128'hFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF;
    end
    else if(tbl_wr_req)
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


  assign lpm_result0 = lpm_table[0][127:64];
  assign lpm_result1= lpm_table[1][127:64];
  assign lpm_result2 = lpm_table[2][127:64];
  assign lpm_result3 = lpm_table[3][127:64];
  assign lpm_result4 = lpm_table[4][127:64];
  assign lpm_result5 = lpm_table[5][127:64];
  assign lpm_result6 = lpm_table[6][127:64];
  assign lpm_result7 = lpm_table[7][127:64];
  assign lpm_result8 = lpm_table[8][127:64];
  assign lpm_result9 = lpm_table[9][127:64];
  assign lpm_result10 = lpm_table[10][127:64];
  assign lpm_result11 = lpm_table[11][127:64];
  assign lpm_result12 = lpm_table[12][127:64];
  assign lpm_result13 = lpm_table[13][127:64];
  assign lpm_result14 = lpm_table[14][127:64];
  assign lpm_result15 = lpm_table[15][127:64];
  assign lpm_result16 = lpm_table[16][127:64];
  assign lpm_result17 = lpm_table[17][127:64];
  assign lpm_result18 = lpm_table[18][127:64];
  assign lpm_result19 = lpm_table[19][127:64];
  assign lpm_result20 = lpm_table[20][127:64];
  assign lpm_result21 = lpm_table[21][127:64];
  assign lpm_result22 = lpm_table[22][127:64];
  assign lpm_result23 = lpm_table[23][127:64];
  assign lpm_result24 = lpm_table[24][127:64];
  assign lpm_result25 = lpm_table[25][127:64];
  assign lpm_result26 = lpm_table[26][127:64];
  assign lpm_result27 = lpm_table[27][127:64];
  assign lpm_result28 = lpm_table[28][127:64];
  assign lpm_result29 = lpm_table[29][127:64];
  assign lpm_result30 = lpm_table[30][127:64];
  assign lpm_result31 = lpm_table[31][127:64];



   fallthrough_small_fifo
        #( .WIDTH(C_M_AXIS_DATA_WIDTH+C_M_AXIS_TUSER_WIDTH+C_M_AXIS_DATA_WIDTH/8+1),
           .MAX_DEPTH_BITS(2))
      input_fifo
        (// Outputs
         .dout                           ({M_AXIS_TLAST, M_AXIS_TUSER, M_AXIS_TSTRB, M_AXIS_TDATA}),
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

   reg lpm_hit_next,arp_hit,lpm_p,lpm_p_next;
   reg [1:0] state;
   reg [31:0] ip_check,ip_temp,mask_temp,queue,lpm_miss_next;
	reg [47:0] dest_mac;
   reg [31:0] ip_mask, net_mask, next_hop, oq, ip_addr_next;

   reg [1:0] header, header_next;
   reg [31:0] a , b,wire_queue,wire_nh,result;
   reg [127:0] table_line;
   reg [4:0] index; 

   reg arp_lnext;
   reg [31:0] nh_next,oq_next;
   reg [4:0] index_next; 

   always@(lpm_table[0],lpm_table[1],lpm_table[2],lpm_table[3],lpm_table[4],lpm_table[5],
lpm_table[6],lpm_table[7],lpm_table[8],lpm_table[9],lpm_table[10],lpm_table[11],lpm_table[12],
lpm_table[13],lpm_table[14],lpm_table[15],lpm_table[16],lpm_table[17],lpm_table[18],lpm_table[19],
lpm_table[20],lpm_table[21],lpm_table[22],lpm_table[23],lpm_table[24],lpm_table[25],lpm_table[26],
lpm_table[27],lpm_table[28],lpm_table[29],lpm_table[30],lpm_table[31],ip_addr,M_AXIS_TREADY,
M_AXIS_TVALID,header,M_AXIS_TUSER, M_AXIS_TLAST, index_hit,lpm_hit )
   begin
     header_next = header;
//     M_AXIS_TUSER   = M_AXIS_TUSER0;
	lpm_hit_next = lpm_hit;
	index_next = index_hit;
	ip_addr_next = ip_addr_out;
     if(header == 2'd0 & M_AXIS_TVALID & !M_AXIS_TLAST & M_AXIS_TREADY )
     begin
	header_next = 2'd1; 
	lpm_hit_next = 0;
	ip_addr_next = ip_addr;
       if( !(M_AXIS_TUSER[DST_PORT_POS+1] || M_AXIS_TUSER[DST_PORT_POS+3] || M_AXIS_TUSER[DST_PORT_POS+5] || M_AXIS_TUSER[DST_PORT_POS+7]) )
       begin
	for(j=31;j>=0;j=j-1)
	 begin
	   if( (ip_addr & lpm_table[j][63:32]) == (lpm_table[j][31:0] & lpm_table[j][63:32] ) ) 
	   begin
	     lpm_hit_next = 1;
	     index_next = j;
	   end
	 end
      end
       end
/*	else if(header == 2'd1 & M_AXIS_TVALID & !M_AXIS_TLAST & M_AXIS_TREADY)
	begin
	header_next = 2;
	  if(lpm_p)
	  begin 
	     lpm_hit_next = 1;
	     oq_next = lpm_table[index_hit][127:96];
	     nh_next = lpm_table[index_hit][95:64];
	  end
	end
 */     else if( header == 2'd1 & M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY)
       begin
	header_next = 0;
/*
	lpm_hit_next = 0;
	lpm_p_next = 0;
	nh_next = 0;
	oq_next = 0;
*/
       end
   end



   always@(posedge AXI_ACLK)
   begin
   if(~AXI_RESETN)
   begin
	ip_addr_out <= 32'd0;
	header <= 2'd0;	
	index_hit <= 5'd0;
	lpm_hit <= 0;
   end
   else 
   begin
	ip_addr_out <= ip_addr_next;
	index_hit <= index_next;
	header <= header_next;
   	lpm_hit <= lpm_hit_next;
   end
   end

endmodule

//	  lpm_miss_next = lpm_miss_count;
//	arp_lnext = arp_lookup;
//	nh_next = nh_reg;
//	oq_next = oq_reg;
//	lpm_p_next = lpm_p;
/*
       else if(	state == 2 & M_AXIS_TREADY & M_AXIS_TVALID0) 
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
*/
