

module drop_packets1
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
    input [C_S_AXI_DATA_WIDTH-1:0] reset,
    input [C_S_AXI_DATA_WIDTH-1:0] mac0_low,
    input [C_S_AXI_DATA_WIDTH-1:0] mac0_high,
    input [C_S_AXI_DATA_WIDTH-1:0] mac1_low,
    input [C_S_AXI_DATA_WIDTH-1:0] mac1_high,
    input [C_S_AXI_DATA_WIDTH-1:0] mac2_low,
    input [C_S_AXI_DATA_WIDTH-1:0] mac2_high,
    input [C_S_AXI_DATA_WIDTH-1:0] mac3_low,
    input [C_S_AXI_DATA_WIDTH-1:0] mac3_high,
    input	[15:0] low_ip_addr,
    input 	[15:0] checksum,
    output reg [31:0] destip_addr,
    output reg [31:0] wrong_mac_count, 
    output reg [31:0] dropped_count,
    output reg [4:0] drop_array
);

    wire [C_M_AXIS_DATA_WIDTH-1:0] 	M_AXIS_TDATA0;
    wire [((C_M_AXIS_DATA_WIDTH/8))-1:0] M_AXIS_TSTRB0;
    wire [C_M_AXIS_TUSER_WIDTH-1:0]      M_AXIS_TUSER0;
    wire 				M_AXIS_TVALID0;
    wire 				M_AXIS_TLAST0;
 
   fallthrough_small_fifo
        #( .WIDTH(C_M_AXIS_DATA_WIDTH+C_M_AXIS_TUSER_WIDTH+C_M_AXIS_DATA_WIDTH/8+1),
           .MAX_DEPTH_BITS(4))
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

  reg header , header_next;
  reg [31:0] checksum11,temp_checksum1;
  reg [15:0] temp_checksum, checksum_final; 
  reg drop,drop_next;
  reg [4:0] drop_array_next;
  reg [31:0] destip_next,dropped_count_next, wrong_mac_count_next;



  // assign M_AXIS_TVALID = drop_next ? 0 : M_AXIS_TVALID0;

  always@* //(posedge AXI_ACLK)
  begin

     header_next = header;
     drop_next = drop;
     wrong_mac_count_next = wrong_mac_count;
     dropped_count_next = dropped_count;
     destip_next = destip_addr;
     drop_array_next = drop_array;
     if(header == 0 & M_AXIS_TVALID &  !M_AXIS_TLAST ) begin
	header_next = 1; 
	drop_array_next = 5'd0;
	if(M_AXIS_TUSER[SRC_PORT_POS] || M_AXIS_TUSER[SRC_PORT_POS+2] || M_AXIS_TUSER[SRC_PORT_POS+4] || M_AXIS_TUSER[SRC_PORT_POS+6] )
	begin //{
	destip_next = {M_AXIS_TDATA[15:0],low_ip_addr};
	if(checksum != M_AXIS_TDATA[63:48]) 
	begin
	  drop_array_next[0] = 1;
	  dropped_count_next = dropped_count_next + 1;
	end
	else 
	begin
	 if(M_AXIS_TUSER[SRC_PORT_POS] && !(M_AXIS_TDATA[255:208] == {mac0_high[15:0],mac0_low} || M_AXIS_TDATA[255:208] == 48'hFFFFFFFFFFFF))
	 begin 
	  drop_array_next[1] = 1;
//	  wrong_mac_count_next = wrong_mac_count_next + 1;  
	 end
	
	 if(M_AXIS_TUSER[SRC_PORT_POS+2] && !(M_AXIS_TDATA[255:208] == {mac1_high[15:0],mac1_low} || M_AXIS_TDATA[255:208] == 48'hFFFFFFFFFFFF))
	 begin 
	  drop_array_next[2] = 1;
//	  wrong_mac_count_next = wrong_mac_count_next + 1;  
	 end
	
	 if(M_AXIS_TUSER[SRC_PORT_POS+6] && !(M_AXIS_TDATA[255:208] == {mac2_high[15:0],mac2_low} || M_AXIS_TDATA[255:208] == 48'hFFFFFFFFFFFF))
	 begin 
	  drop_array_next[3] = 1;
//	  wrong_mac_count_next = wrong_mac_count_next + 1;  
	 end
	
	 if(M_AXIS_TUSER[SRC_PORT_POS+8] && !(M_AXIS_TDATA[255:208] == {mac3_high[15:0],mac3_low} || M_AXIS_TDATA[255:208] == 48'hFFFFFFFFFFFF))
	 begin 
	  drop_array_next[4] = 1;
//	  wrong_mac_count_next = wrong_mac_count_next + 1;  
 	 end
     end
	if(drop_array_next[4:1] != 4'd0) wrong_mac_count_next = wrong_mac_count_next + 1;	
//	if(drop_array != 5'd0) drop_next = 1;
//	M_AXIS_TVALID = drop_next ? 0 : M_AXIS_TVALID0;
     end //}
	end
     else if(header == 1 & M_AXIS_TVALID & M_AXIS_TLAST & M_AXIS_TREADY )
     begin
//	M_AXIS_TVALID = drop_next ? 0 : M_AXIS_TVALID0;
	header_next = 0;
//	drop_next = 0;
     end
  end


  always@(posedge AXI_ACLK)
  begin
     if(~AXI_RESETN) begin
	header <= 0;
	dropped_count <= 0;
	wrong_mac_count <= 0;
	drop_array <= 0;
	destip_addr <= 0;
     end
     else if(reset == 32'd1)
     begin
	dropped_count <= 0;
	wrong_mac_count <= 0;
	header <= header_next;
	drop_array <= drop_array_next;
	destip_addr <= destip_next;
     end 
     else 
     begin
	dropped_count <= dropped_count_next;
	wrong_mac_count <= wrong_mac_count_next;
	header <= header_next;
	drop_array <= drop_array_next;
	destip_addr <= destip_next;
     end
  end

endmodule

