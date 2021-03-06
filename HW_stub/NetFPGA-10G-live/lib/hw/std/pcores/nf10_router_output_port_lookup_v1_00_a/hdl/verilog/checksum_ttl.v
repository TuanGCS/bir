

module checksum_ttl
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
    output reg [C_M_AXIS_TUSER_WIDTH-1:0]M_AXIS_TUSER,
    output 				M_AXIS_TVALID,
    input  				M_AXIS_TREADY,
    output				M_AXIS_TLAST,

    // Slave Stream Ports (interface to RX queues)
    input [C_S_AXIS_DATA_WIDTH-1:0] 	S_AXIS_TDATA,
    input [((C_S_AXIS_DATA_WIDTH/8))-1:0]S_AXIS_TSTRB,
    input [C_S_AXIS_TUSER_WIDTH-1:0] 	S_AXIS_TUSER,
    input  				S_AXIS_TVALID,
    output 				S_AXIS_TREADY,
    input  				S_AXIS_TLAST
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

/*
 always@(posedge AXI_ACLK)
  begin
    M_AXIS_TDATA  <= M_AXIS_TDATA0;
    M_AXIS_TSTRB  <= M_AXIS_TSTRB0;
    // M_AXIS_TUSER1  <= M_AXIS_TUSER0;
    M_AXIS_TVALID <= M_AXIS_TVALID0;
    M_AXIS_TLAST  <= M_AXIS_TLAST0;
  end
*/
  reg [31:0] reset, bad_ttl_count_next, non_ip_count_next, ver_count_next;
  reg header_next;
  reg cpu_hit;
  reg [31:0] bad_ttl_count, non_ip_count, ver_count;
  reg header;

  always@(posedge AXI_ACLK)
  begin
     if(~AXI_RESETN) begin
	bad_ttl_count <= 0;
	ver_count <= 0;
	header <= 0;
	non_ip_count <= 0;
     end
     else if(reset == 1)
     begin
	bad_ttl_count <= 0;
	ver_count <= 0;
	header <= header_next;
	non_ip_count <= 0;
     end
     else
     begin
	bad_ttl_count <= bad_ttl_count_next;
	ver_count <= ver_count_next;
	header <= header_next;
	non_ip_count <= non_ip_count_next;
     end
  end

  always@* //(posedge AXI_ACLK)
  begin
     header_next = header;
     M_AXIS_TUSER = M_AXIS_TUSER0;
     if(header == 0 & M_AXIS_TVALID) begin
	cpu_hit = 0;
	header_next = 1;
	if(M_AXIS_TDATA[79:72] < 1) // Check TTL
	begin
	  cpu_hit = 1;
	  bad_ttl_count_next = bad_ttl_count_next + 1;
	end
	if(M_AXIS_TDATA[143:140] != 4'd4 )
	begin
	  cpu_hit = 1;
	  ver_count_next = ver_count_next + 1;
	end
	if(M_AXIS_TDATA[159:144] != 16'h0800)	
	begin
	  cpu_hit = 1;
	  non_ip_count_next = non_ip_count_next + 1;
	end
	
	if(cpu_hit)
	begin
          if(M_AXIS_TUSER0[SRC_PORT_POS])   M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] =   8'b00000010;
          if(M_AXIS_TUSER0[SRC_PORT_POS+2]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00001000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+4]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00100000;
          if(M_AXIS_TUSER0[SRC_PORT_POS+6]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b10000000;
	end
	cpu_hit = 0;
    end
    else if(header == 1 & M_AXIS_TLAST & M_AXIS_TVALID)
    begin
      header_next = 0;
      cpu_hit = 0;
    end 
  end



endmodule

