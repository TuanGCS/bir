

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
    M_AXIS_TDATA  <= M_AXIS_TDATA0;
    M_AXIS_TSTRB  <= M_AXIS_TSTRB0;
    M_AXIS_TUSER  <= M_AXIS_TUSER0;
    M_AXIS_TVALID <= M_AXIS_TVALID0;
    M_AXIS_TLAST  <= M_AXIS_TLAST0;
  end


/*
  always@(posedge AXI_ACLK)
  begin
      if(~AXI_RESETN) begin
	 c_state <= 0;
         ipv4_count <= 0;
         arp_count <= 0;
         ospf_count <= 0;
      end
      else if(!c_state & M_AXIS_TVALID & M_AXIS_TREADY) begin
	 c_state <= 1;
         if(M_AXIS_TDATA[159:144] == 16'h0806) arp_count <= arp_count + 1;
	 else if(M_AXIS_TDATA[159:144] == 16'h0800) begin 
           if(M_AXIS_TDATA[143:140] == 4'd4) ipv4_count <= ipv4_count + 1;
	   if(M_AXIS_TDATA[71:64] == 8'd89) ospf_count <= ospf_count + 1;
         end
      end
      else if(c_state & M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY) c_state <= 0;
  end
*/





endmodule

