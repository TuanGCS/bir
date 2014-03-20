

module lpm2
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
//    input [C_S_AXI_DATA_WIDTH-1:0]	ip_addr,
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
//    output reg arp_lookup,
    output reg [31:0] nh_reg,
    output reg [31:0] oq_reg,
    input lpm_hit_in,
    input [4:0] index_hit_in,
    input [63:0] lpm_result0, lpm_result1, lpm_result2, lpm_result3, lpm_result4, lpm_result5, lpm_result6, lpm_result7, 
lpm_result8, lpm_result9, lpm_result10, lpm_result11, lpm_result12, lpm_result13, lpm_result14, lpm_result15, 
lpm_result16, lpm_result17, lpm_result18, lpm_result19, lpm_result20, lpm_result21, lpm_result22, lpm_result23, 
lpm_result24, lpm_result25, lpm_result26, lpm_result27, lpm_result28, lpm_result29, lpm_result30, lpm_result31
);

    wire [C_S_AXI_DATA_WIDTH*2-1:0] lpm_result [0:31];      // Value in table

    assign lpm_result[0] = lpm_result0;
    assign lpm_result[1] = lpm_result1;
    assign lpm_result[2] = lpm_result2;
    assign lpm_result[3] = lpm_result3;
    assign lpm_result[4] = lpm_result4;
    assign lpm_result[5] = lpm_result5;
    assign lpm_result[6] = lpm_result6;
    assign lpm_result[7] = lpm_result7;
    assign lpm_result[8] = lpm_result8;
    assign lpm_result[9] = lpm_result9;
    assign lpm_result[10] = lpm_result10;
    assign lpm_result[11] = lpm_result11;
    assign lpm_result[12] = lpm_result12;
    assign lpm_result[13] = lpm_result13;
    assign lpm_result[14] = lpm_result14;
    assign lpm_result[15] = lpm_result15;
    assign lpm_result[16] = lpm_result16;
    assign lpm_result[17] = lpm_result17;
    assign lpm_result[18] = lpm_result18;
    assign lpm_result[19] = lpm_result19;
    assign lpm_result[20] = lpm_result20;
    assign lpm_result[21] = lpm_result21;
    assign lpm_result[22] = lpm_result22;
    assign lpm_result[23] = lpm_result23;
    assign lpm_result[24] = lpm_result24;
    assign lpm_result[25] = lpm_result25;
    assign lpm_result[26] = lpm_result26;
    assign lpm_result[27] = lpm_result27;
    assign lpm_result[28] = lpm_result28;
    assign lpm_result[29] = lpm_result29;
    assign lpm_result[30] = lpm_result30;
    assign lpm_result[31] = lpm_result31;


  wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA0;
  wire [((C_M_AXIS_DATA_WIDTH/8))-1:0] M_AXIS_TSTRB0;
  wire [C_M_AXIS_TUSER_WIDTH-1:0]      M_AXIS_TUSER0;
  wire 				M_AXIS_TVALID0;
  wire 				M_AXIS_TLAST0;


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
   reg [31:0] ip_mask, net_mask, next_hop, oq;

   reg [1:0] header, header_next;
   reg [31:0] a , b,wire_queue,wire_nh,result;
   reg [127:0] table_line;
   reg [4:0] index; 

   reg arp_lnext;
   reg [31:0] nh_next,oq_next;
   reg [4:0] index_next, index_hit; 

   always@(lpm_result[0],lpm_result[1],lpm_result[2],lpm_result[3],lpm_result[4],lpm_result[5],
lpm_result[6],lpm_result[7],lpm_result[8],lpm_result[9],lpm_result[10],lpm_result[11],lpm_result[12],
lpm_result[13],lpm_result[14],lpm_result[15],lpm_result[16],lpm_result[17],lpm_result[18],lpm_result[19],
lpm_result[20],lpm_result[21],lpm_result[22],lpm_result[23],lpm_result[24],lpm_result[25],lpm_result[26],
lpm_result[27],lpm_result[28],lpm_result[29],lpm_result[30],lpm_result[31],M_AXIS_TREADY,
M_AXIS_TVALID,header,M_AXIS_TUSER,M_AXIS_TLAST,lpm_p, index_hit )
   begin
     header_next = header;
//     M_AXIS_TUSER   = M_AXIS_TUSER0;
	nh_next = nh_reg;
	oq_next = oq_reg;
	lpm_hit_next = lpm_hit;
     if(header == 2'd0 & M_AXIS_TVALID & !M_AXIS_TLAST )
     begin
       header_next = 2'd1; 
       if( !(M_AXIS_TUSER[DST_PORT_POS+1] || M_AXIS_TUSER[DST_PORT_POS+3] || M_AXIS_TUSER[DST_PORT_POS+5] || M_AXIS_TUSER[DST_PORT_POS+7]) )
       begin
	  if(lpm_hit_in)
	  begin 
	     oq_next = lpm_result[index_hit_in][63:32];
	     nh_next = lpm_result[index_hit_in][31:0];
	  end
	  lpm_hit_next = lpm_hit_in;
	end
      end
      else if( header == 2'd1 & M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY)
      begin
	header_next = 0;
      end
   end



   always@(posedge AXI_ACLK)
   begin
   if(~AXI_RESETN)
   begin
     header <= 2'd0;
     nh_reg <= 0;
     oq_reg <= 0;
     lpm_hit <= 0;
   end
   else 
   begin
     header <= header_next;
     nh_reg <= nh_next;
     oq_reg <= oq_next;
     lpm_hit <= lpm_hit_next;
   end

   end

endmodule

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
