

module first_stage
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
    output reg [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA,
    output reg[((C_M_AXIS_DATA_WIDTH/8))-1:0]M_AXIS_TSTRB,
    output reg [C_M_AXIS_TUSER_WIDTH-1:0]M_AXIS_TUSER,
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

//    output reg [C_S_AXI_DATA_WIDTH-1:0] ipv4_count,
//    output reg [C_S_AXI_DATA_WIDTH-1:0] arp_count,
//    output reg [C_S_AXI_DATA_WIDTH-1:0] ospf_count
    input [C_S_AXI_DATA_WIDTH-1:0] reset,
    input [C_S_AXI_DATA_WIDTH-1:0] mac0_low,
    input [C_S_AXI_DATA_WIDTH-1:0] mac0_high,
    input [C_S_AXI_DATA_WIDTH-1:0] mac1_low,
    input [C_S_AXI_DATA_WIDTH-1:0] mac1_high,
    input [C_S_AXI_DATA_WIDTH-1:0] mac2_low,
    input [C_S_AXI_DATA_WIDTH-1:0] mac2_high,
    input [C_S_AXI_DATA_WIDTH-1:0] mac3_low,
    input [C_S_AXI_DATA_WIDTH-1:0] mac3_high,
    output reg [C_S_AXI_DATA_WIDTH-1:0] wrong_mac_count,
    output reg [C_S_AXI_DATA_WIDTH-1:0] non_ip_count,
    output reg [C_S_AXI_DATA_WIDTH-1:0] dropped_count,
    output reg [C_S_AXI_DATA_WIDTH-1:0] bad_ttl_count,
    output reg [C_S_AXI_DATA_WIDTH-1:0] ver_count,
    output reg [C_S_AXI_DATA_WIDTH-1:0] cpu_count,
    output reg [C_S_AXI_DATA_WIDTH-1:0] dest_hit_count,
    output reg [C_S_AXI_DATA_WIDTH-1:0] ip_addr,
    input tbl_rd_req,       // Request a read
    input tbl_wr_req,       // Request a write
    input 	[4:0] tbl_rd_addr,      // Address in table to read
    input 	[4:0] tbl_wr_addr,      // Address in table to write
    input 	[C_S_AXI_DATA_WIDTH-1:0] tbl_wr_data,      // Value to write to table
    output reg 	[C_S_AXI_DATA_WIDTH-1:0] tbl_rd_data,      // Value in table
    output reg tbl_wr_ack,       // Pulses hi on ACK
    output reg tbl_rd_ack      // Pulses hi on ACK

);

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA0;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0] M_AXIS_TSTRB0;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]      M_AXIS_TUSER0;
    wire 				M_AXIS_TVALID0;
    wire 				M_AXIS_TLAST0;
    reg [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA1;
    reg [((C_M_AXIS_DATA_WIDTH/8))-1:0] M_AXIS_TSTRB1;
    reg [C_M_AXIS_TUSER_WIDTH-1:0]      M_AXIS_TUSER1;
    reg 				M_AXIS_TVALID1;
    reg 				M_AXIS_TLAST1;
    reg [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA2;
    reg [((C_M_AXIS_DATA_WIDTH/8))-1:0] M_AXIS_TSTRB2;
    reg [C_M_AXIS_TUSER_WIDTH-1:0]      M_AXIS_TUSER2;
    reg 				M_AXIS_TVALID2;
    reg 				M_AXIS_TLAST2;



    reg	[C_S_AXI_DATA_WIDTH-1:0] dest_ip_table [31:0];      // Value in table


  always@(posedge AXI_ACLK)
  begin
    if(tbl_wr_req)
    begin
      tbl_wr_ack <= 1;
      dest_ip_table[tbl_wr_addr] <= tbl_wr_data;
    end
    else tbl_wr_ack <= 0; 
  end


  always@(posedge AXI_ACLK)
  begin
    if(tbl_rd_req)
    begin
      tbl_rd_ack <= 1;
      tbl_rd_data <= dest_ip_table[tbl_rd_addr];
    end
    else tbl_rd_ack <= 0; 
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

  always@(posedge AXI_ACLK)
  begin
    M_AXIS_TDATA  <= M_AXIS_TDATA2;
    M_AXIS_TSTRB  <= M_AXIS_TSTRB2;
//    M_AXIS_TUSER  <= M_AXIS_TUSER2;
    M_AXIS_TVALID <= M_AXIS_TVALID2;
    M_AXIS_TLAST  <= M_AXIS_TLAST2;
  end

 always@(posedge AXI_ACLK)
  begin
    M_AXIS_TDATA2  <= M_AXIS_TDATA1;
    M_AXIS_TSTRB2  <= M_AXIS_TSTRB1;
    M_AXIS_TUSER2  <= M_AXIS_TUSER1;
    M_AXIS_TVALID2 <= !drop ? M_AXIS_TVALID1 : 0;
    M_AXIS_TLAST2  <= M_AXIS_TLAST1;
  end

 always@(posedge AXI_ACLK)
  begin
    M_AXIS_TDATA1  <= M_AXIS_TDATA0;
    M_AXIS_TSTRB1  <= M_AXIS_TSTRB0;
    // M_AXIS_TUSER1  <= M_AXIS_TUSER0;
    M_AXIS_TVALID1 <= M_AXIS_TVALID0;
    M_AXIS_TLAST1  <= M_AXIS_TLAST0;
  end



 reg [1:0] crc_state;
 reg [191:0] crc_data;
 reg [15:0] checksum;
 reg [31:0] temp;
 reg [15:0] temp1;
 reg [15:0] temp2;
 reg [31:0] temp3;
 reg drop,drop1,drop2;

 integer i;

  always@(posedge AXI_ACLK)
  begin
     if(~AXI_RESETN) begin
	crc_state <= 0;
        checksum <= 0;
	temp <= 0;
	dropped_count <= 0;
	cpu_count <= 0;
	wrong_mac_count <= 0;
     end
     else if(reset == 1)
     begin
	dropped_count <= 0;
	cpu_count <= 0;
	wrong_mac_count <= 0;
     end
     else if(crc_state == 2'd0 & M_AXIS_TREADY & M_AXIS_TVALID0) 
     begin
//		if(M_AXIS_TDATA0[79:72] > 1) // Check TTL
//		begin
		// Compute Checksum if TTL is good
			crc_state <= 1;
			temp <= M_AXIS_TDATA0[143:128] + M_AXIS_TDATA0[127:112] + M_AXIS_TDATA0[111:96] 
			        + M_AXIS_TDATA0[95:80] + M_AXIS_TDATA0[79:64] + M_AXIS_TDATA0[47:32]  
			        + M_AXIS_TDATA0[31:16] + M_AXIS_TDATA0[15:0];
//					  drop1 <= 0;
//		end
//		else 
//		begin
//			drop1 <= 1; // Drop if bad TTL
			crc_state <= 1;
//			temp <= 0;
     end
     else if(crc_state == 2'd1 & M_AXIS_TREADY & M_AXIS_TVALID0)
     begin
	crc_state <= 2;
	// temp2 = M_AXIS_TDATA0[255:240];
	temp3 = temp + M_AXIS_TDATA0[255:240];
	temp1 = temp3[15:0] + temp3[19:16];
	checksum = ~temp1;
	if(checksum != M_AXIS_TDATA1[63:48]) 
	begin
	  drop <= 1; // Drop if Wrong Checksum
	  // drop1 <= 1;
	  dropped_count <= dropped_count + 1;
	end
	else 
	begin

	 if(M_AXIS_TUSER1[SRC_PORT_POS] && !(M_AXIS_TDATA1[255:208] == {mac0_high[15:0],mac0_low} || M_AXIS_TDATA1 == 48'hFFFFFFFFFFFF))
	 begin 
	  drop <= 1;
	  wrong_mac_count <= wrong_mac_count + 1;  
	 end
	
	 if(M_AXIS_TUSER1[SRC_PORT_POS+2] && !(M_AXIS_TDATA1[255:208] == {mac1_high[15:0],mac1_low} || M_AXIS_TDATA1 == 48'hFFFFFFFFFFFF))
	 begin 
	  drop <= 1;
	  wrong_mac_count <= wrong_mac_count + 1;  
	 end
	
	 if(M_AXIS_TUSER1[SRC_PORT_POS+6] && !(M_AXIS_TDATA1[255:208] == {mac2_high[15:0],mac2_low} || M_AXIS_TDATA1 == 48'hFFFFFFFFFFFF))
	 begin 
	  drop <= 1;
	  wrong_mac_count <= wrong_mac_count + 1;  
	 end
	
	 if(M_AXIS_TUSER1[SRC_PORT_POS+8] && !(M_AXIS_TDATA1[255:208] == {mac3_high[15:0],mac3_low} || M_AXIS_TDATA1 == 48'hFFFFFFFFFFFF))
	 begin 
	  drop <= 1;
	  wrong_mac_count <= wrong_mac_count + 1;  
 	 end
	
	end
      end
      else if(crc_state == 2'd2 & M_AXIS_TLAST0 & M_AXIS_TVALID0 & M_AXIS_TREADY) begin 
         crc_state <= 0;
//	 drop1 <= 0;
	 // drop <= 1; // drop1;
     end
     else if( !M_AXIS_TVALID0)
     begin
      drop <= 0 ; // drop1;
//      drop1 <= 0;
     end
  end


  reg header;
  reg cpu_hit;

  reg header1;	
  reg [31:0] ip;
  reg [31:0] ip_data_check;
  reg [1:0]  state1;
  reg ip_hit, ip_hit_reg;
//  integer i;
//  reg cpu_hit;

  always@(posedge AXI_ACLK)
  begin
     M_AXIS_TUSER = M_AXIS_TUSER2;
     if(~AXI_RESETN) begin
	ip <= 0;
	dest_hit_count <= 0;
	state1 <= 0;
     end
     else if(reset == 1)
     begin
	dest_hit_count <= 0;
     end
     else if(state1 == 0 & M_AXIS_TREADY & M_AXIS_TVALID0) begin
	ip <= {M_AXIS_TDATA0[15:0],16'd0};
	state1 <= 1;
     end
     else if(state1 == 1 & M_AXIS_TREADY & M_AXIS_TVALID0) begin
	state1 <= 2;
	ip_data_check = {ip[31:16],M_AXIS_TDATA0[255:240]};
	ip_hit = 0;
	for(i=0; i<32; i=i+1)
	begin
	  if(!ip_hit)
	  begin
	    if(ip_data_check == dest_ip_table[i]) ip_hit = 1;
	  end
	end
	ip_hit_reg <= ip_hit;
	ip_addr <= ip_data_check;
     end
     else if(state1 == 2)
     begin 
	state1 <= 3;
	if(ip_hit_reg)
	begin
	  dest_hit_count <= dest_hit_count + 1;
          if(M_AXIS_TUSER2[SRC_PORT_POS])   M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] =   8'b00000010;
          if(M_AXIS_TUSER2[SRC_PORT_POS+2]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00001000;
          if(M_AXIS_TUSER2[SRC_PORT_POS+4]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00100000;
          if(M_AXIS_TUSER2[SRC_PORT_POS+6]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b10000000;
	end
	ip_hit_reg <= 0;
     end
    else if(state1 == 3 & M_AXIS_TLAST1 & M_AXIS_TREADY & M_AXIS_TVALID1)
    begin
      state1 <= 0;
      ip_hit_reg <= 0;
    end 
  end



endmodule



/*
  always@(posedge AXI_ACLK)
  begin
     M_AXIS_TUSER1 = M_AXIS_TUSER0;
     if(~AXI_RESETN) begin
	bad_ttl_count <= 0;
	ver_count <= 0;
	header <= 0;
	cpu_hit = 0;
	non_ip_count <= 0;
     end
     else if(reset == 1)
     begin
	bad_ttl_count <= 0;
	ver_count <= 0;
	//header <= 0;
	non_ip_count <= 0;
     end
     else if(header == 0 & M_AXIS_TREADY & M_AXIS_TVALID0) begin
	cpu_hit = 0;
	header <= 1;
	if(M_AXIS_TDATA0[79:72] < 1) // Check TTL
	begin
	  cpu_hit = 1;
	  bad_ttl_count <= bad_ttl_count + 1;
	end
	if(M_AXIS_TDATA0[143:140] != 4'd4 )
	begin
	  cpu_hit = 1;
	  ver_count <= ver_count + 1;
	end
	if(M_AXIS_TDATA0[159:144] != 16'h0800)	
	begin
	  cpu_hit = 1;
	  non_ip_count <= non_ip_count + 1;
	end
	
	if(cpu_hit)
	begin
          if(M_AXIS_TUSER0[SRC_PORT_POS])   M_AXIS_TUSER1[DST_PORT_POS+7:DST_PORT_POS] =   8'b00000010;
          if(M_AXIS_TUSER0[SRC_PORT_POS+2]) M_AXIS_TUSER1[DST_PORT_POS+7:DST_PORT_POS] = 8'b00001000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+4]) M_AXIS_TUSER1[DST_PORT_POS+7:DST_PORT_POS] = 8'b00100000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+6]) M_AXIS_TUSER1[DST_PORT_POS+7:DST_PORT_POS] = 8'b10000000;
	end
	cpu_hit = 0;
    end
   else if(header == 1 & M_AXIS_TLAST0 & M_AXIS_TREADY & M_AXIS_TVALID0)
    begin
      header <= 0;
      cpu_hit = 0;
    end 
  end
*/

