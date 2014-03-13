

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
    output [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA,
    output [((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB,
    output reg[C_M_AXIS_TUSER_WIDTH-1:0]M_AXIS_TUSER,
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
    output reg arp_lookup,
    output reg [31:0] nh_reg,
    output reg [31:0] oq_reg
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


   fallthrough_small_fifo
        #( .WIDTH(C_M_AXIS_DATA_WIDTH+C_M_AXIS_TUSER_WIDTH+C_M_AXIS_DATA_WIDTH/8+1),
           .MAX_DEPTH_BITS(2))
      input_fifo
        (// Outputs
         .dout                           ({M_AXIS_TLAST, M_AXIS_TUSER0, M_AXIS_TSTRB, M_AXIS_TDATA}),
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

   reg lpm_hit,arp_hit;
   reg [1:0] state;
   reg [31:0] ip_check,ip_temp,mask_temp,queue,lpm_miss_next;
	reg [47:0] dest_mac;
   reg [31:0] ip_mask, net_mask, next_hop, oq;

   reg header, header_next;
   reg [31:0] a , b,wire_queue,wire_nh,result;
   reg [127:0] table_line;
   reg [4:0] index; 

   reg arp_lnext;
   reg [31:0] nh_next,oq_next; 

   always@(lpm_table[0],lpm_table[1],lpm_table[2],lpm_table[3],lpm_table[4],lpm_table[5],
lpm_table[6],lpm_table[7],lpm_table[8],lpm_table[9],lpm_table[10],lpm_table[11],lpm_table[12],
lpm_table[13],lpm_table[14],lpm_table[15],lpm_table[16],lpm_table[17],lpm_table[18],lpm_table[19],
lpm_table[20],lpm_table[21],lpm_table[22],lpm_table[23],lpm_table[24],lpm_table[25],lpm_table[26],
lpm_table[27],lpm_table[28],lpm_table[29],lpm_table[30],lpm_table[31],ip_addr,M_AXIS_TREADY,M_AXIS_TVALID,lpm_miss_count,header,M_AXIS_TUSER0,M_AXIS_TLAST )
   begin
     header_next = header;
     M_AXIS_TUSER   = M_AXIS_TUSER0;
	  lpm_miss_next = lpm_miss_count;
	arp_lnext = arp_lookup;
	nh_next = nh_reg;
	oq_next = oq_reg;

     if(header == 0 & M_AXIS_TVALID & !M_AXIS_TLAST )
     begin
       header_next = 1; 
       if( !(M_AXIS_TUSER0[DST_PORT_POS+1] || M_AXIS_TUSER0[DST_PORT_POS+3] || M_AXIS_TUSER0[DST_PORT_POS+5] || M_AXIS_TUSER0[DST_PORT_POS+7]) )
       begin
	 // ip_check = ip_addr;
	 ip_mask = ip_addr;
	 net_mask = 0;
	 lpm_hit = 0;
//	 arp_lookup = 0;
//	 nh_next = 0;
//	 oq_next = 0;
	 result = 0;

   for(j=7;j>=0;j=j-1)
	 begin
	   table_line = lpm_table[j];
	   ip_temp = table_line[31:0];
	   mask_temp = table_line[63:32];
	   a = ip_temp;
	   b = ip_mask & mask_temp;
	   wire_queue = table_line[127:96];
	   wire_nh =  table_line[95:64];

	   if( ( a == b ) ) 
	   begin
//		result[j] = 1;

//	     ip_mask = ip_temp; 
//	     net_mask = mask_temp;
	     lpm_hit = 1;
	     oq = wire_queue;
	     next_hop = wire_nh;
//	     end
	   end
	 end
/*
	if(result !=0)
	begin
	lpm_hit = 1;
	index = 0;
	if(result[0]==1) index = 0;
	else if(result[1]==1) index = 1;
	else if(result[2]==1) index = 2;
	else if(result[3]==1) index = 3;
	else if(result[4]==1) index = 4;
	else if(result[5]==1) index = 5;
	else if(result[6]==1) index = 6;
	else if(result[7]==1) index = 7;
	else if(result[8]==1) index = 8;
	else if(result[9]==1) index = 9;
	else if(result[10]==1) index = 10;
	else if(result[11]==1) index = 11;
	else if(result[12]==1) index = 12;
	else if(result[13]==1) index = 13;
	else if(result[14]==1) index = 14;
	else if(result[15]==1) index = 15;
	else if(result[16]==1) index = 16;
	else if(result[17]==1) index = 17;
	else if(result[18]==1) index = 18;
	else if(result[19]==1) index = 19;
	else if(result[20]==1) index = 20;
	else if(result[21]==1) index = 21;
	else if(result[22]==1) index = 22;
	else if(result[23]==1) index = 23;
	else if(result[24]==1) index = 24;
	else if(result[25]==1) index = 25;
	else if(result[26]==1) index = 26;
	else if(result[27]==1) index = 27;
	else if(result[28]==1) index = 28;
	else if(result[29]==1) index = 29;
	else if(result[30]==1) index = 30;
	else if(result[31]==1) index = 31;

	oq = lpm_table[index][127:96];
	next_hop =  lpm_table[index][95:64];

	end
*/

	if(!lpm_hit)
	begin
	  lpm_miss_next = lpm_miss_next + 1;
          if(M_AXIS_TUSER0[SRC_PORT_POS])   M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] =   8'b00000010;
          if(M_AXIS_TUSER0[SRC_PORT_POS+2]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00001000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+4]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00100000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+6]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b10000000;
	end
	else
	begin
 	  oq_next = oq;	
	  nh_next = next_hop;
	  arp_lnext = lpm_hit;
	end
       end
       end
       else if( header == 1 & M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY)
       begin
	header_next = 0;
	arp_lnext = 0;
	nh_next = 0;
	oq_next = 0;
       end
   end



   always@(posedge AXI_ACLK)
   begin
   if(~AXI_RESETN)
   begin
     header <= 0;
     lpm_miss_count <= 0;
     arp_lookup <= 0;
     nh_reg <= 0;
     oq_reg <= 0;
   end
   else if(reset == 32'd1)
   begin
     lpm_miss_count <= 0;
   end
   else 
   begin
     lpm_miss_count <= lpm_miss_next;
     header <= header_next;
     arp_lookup <= arp_lnext;
     nh_reg <= nh_next;
     oq_reg <= oq_next;
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
