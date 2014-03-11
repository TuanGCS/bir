

module ip_addr_checksum_delay
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
    input [31:0] reset,
    output reg [31:0] cpu_count,
    output reg	[15:0] low_ip_addr
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

  reg [1:0] header , header_next;
  reg [31:0] cpu_count_next;
  
  always@* //(posedge AXI_ACLK)
  begin
     cpu_count_next = cpu_count;
     header_next = header;
     if(header == 2'd0 & M_AXIS_TVALID & M_AXIS_TREADY) begin
	header_next = 1;
	if(M_AXIS_TUSER[SRC_PORT_POS+1] || M_AXIS_TUSER[SRC_PORT_POS+3] || M_AXIS_TUSER[SRC_PORT_POS+5] || M_AXIS_TUSER[SRC_PORT_POS+7] )
	begin
	  cpu_count_next = cpu_count_next + 1;
	end 
     end
     else if(header == 2'd1 & M_AXIS_TVALID & !M_AXIS_TLAST & M_AXIS_TREADY)
     begin
	header_next = 2;
        low_ip_addr = M_AXIS_TDATA[255:240];
     end
     else if(header == 2'd1 & M_AXIS_TVALID & M_AXIS_TLAST & M_AXIS_TREADY)
     begin
	header_next = 0;
        low_ip_addr = M_AXIS_TDATA[255:240];
     end
     else if(header == 2'd2 & M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY)
     begin
        header_next = 0;
 //       low_ip_addr = 16'd0;
     end 
  end


  always@(posedge AXI_ACLK)
  begin
     if(~AXI_RESETN) begin
	header <= 0;
	cpu_count <= 0;
     end
     else if(reset == 32'd1)
     begin
	cpu_count <= 0;	
     end
     else
     begin
	header <= header_next;
	cpu_count <= cpu_count_next;
     end
  end

endmodule

