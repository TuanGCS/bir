

module checksum2
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
    input [31:0] checksum01,
    input [31:0] checksum02,
    input [31:0] checksum03,
    input [31:0] checksum04,
    output reg [31:0] checksum11,
    output reg [31:0] checksum12,
//    input [31:0] reset,
//    output reg [31:0] cpu_count,
    output reg	[15:0] low_ip_addr
//    output reg [31:0] partial_checksum1,
//    output reg [31:0] partial_checksum2
/*
    output reg [C_S_AXI_DATA_WIDTH-1:0] ipv4_count,
    output reg [C_S_AXI_DATA_WIDTH-1:0] arp_count,
    output reg [C_S_AXI_DATA_WIDTH-1:0] ospf_count
 */
);

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

//   assign low_ip_addr = M_AXIS_TDATA[255:240];

  reg header , header_next, header2, header2_next;
  reg [31:0] cpu_count_next;
  reg [31:0] cs1,cs2;
  reg [15:0] low_ip_addr_next;

  always@* //(posedge AXI_ACLK)
  begin
     header_next = header;
     cs1 = checksum11;
     cs2 = checksum12;
     if(header == 0 & M_AXIS_TVALID & !M_AXIS_TLAST & M_AXIS_TREADY) begin
	header_next = 1;
	cs1 = checksum01 + checksum02;
	cs2 = checksum03 + checksum04;
     end
/*
     else if(header == 2'd1 & M_AXIS_TVALID & M_AXIS_TREADY)
     begin
//		header_next = 2'd2;
		if(M_AXIS_TLAST) 
		begin
		header_next = 2'd0;
		end
		else 
		begin
		header_next = 2'd2;
		end
		low_ip_addr = M_AXIS_TDATA[255:240];
     end
*/
     else if(header == 1 & M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY)
     begin
        header_next = 0;
//	low_ip_addr = M_AXIS_TDATA[255:240];
//	checksum_next1 = 0;
//	checksum_next2 = 0;
//      low_ip_addr = 16'd0;
     end 
  end

  //reg header2_next, header2;

  always@*
  begin
    header2_next = header2;
    // low_ip_addr_next = low_ip_addr;
    if(header == 1 & header2 == 0 & M_AXIS_TVALID & M_AXIS_TREADY) 
    begin
	low_ip_addr = M_AXIS_TDATA[255:240];
	header2_next = 1;
    end 
    else if(header == 0)
    begin
	header2_next = 0;
    end
  end

/*
  always@*
  begin
    header2_next = header2;
    if(~AXI_RESETN)
    begin
	low_ip_addr = 16'd0;
	header2_next = 0;
    end
    else if(header == 1 & header2 == 0 & M_AXIS_TVALID & M_AXIS_TREADY) 
    begin
	header2_next = 1;
	low_ip_addr = M_AXIS_TDATA[255:240];
    end
    else if(header == 0 & header2 == 1)
    begin
	header2_next = 0;
    end
  end
*/


  always@(posedge AXI_ACLK)
  begin
     if(~AXI_RESETN) begin
	header <= 0;
	header2 <= 0;
//	low_ip_addr <= 16'd0;
	checksum11 <= 0;
	checksum12 <= 0;
     end
     else
     begin
	header <= header_next;
	header2 <= header2_next;
//	low_ip_addr <= low_ip_addr_next;
	checksum11 <= cs1;
	checksum12 <= cs2;
     end
  end

endmodule

/*
  always@* //(posedge AXI_ACLK)
  begin
     header2_next = header2;
     if(header2 == 2'd0 & M_AXIS_TVALID & !M_AXIS_TLAST ) begin
	header2_next = 2'd1;
     end
     else if(header2 == 2'd1 & M_AXIS_TVALID & M_AXIS_TREADY)
     begin
	if(!M_AXIS_TLAST) 
	begin 
	  header2_next = 2'd2;
	end
	else 
	begin
	  header2_next = 2'd0;
	end
	low_ip_addr = M_AXIS_TDATA[255:240];
     end
*/
/*
     else if(header == 2'd1 & M_AXIS_TVALID & !M_AXIS_TLAST & M_AXIS_TREADY)
     begin
	header_next = 2'd2;
	low_ip_addr = M_AXIS_TDATA[255:240];
     end
*/
/*
     else if(header2 == 2'd2 & M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY)
     begin
        header2_next = 2'd0;
//	low_ip_addr = M_AXIS_TDATA[255:240];
//	checksum_next1 = 0;
//	checksum_next2 = 0;
//      low_ip_addr = 16'd0;
     end 
  end
*/


