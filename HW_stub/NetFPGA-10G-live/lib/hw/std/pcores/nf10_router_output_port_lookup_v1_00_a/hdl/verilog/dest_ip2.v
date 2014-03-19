

module dest_ip2
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
    parameter DST_PORT_POS=24,
    parameter LOOP = 32
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
    output 				M_AXIS_TLAST,

    // Slave Stream Ports (interface to RX queues)
    input [C_S_AXIS_DATA_WIDTH-1:0] 	S_AXIS_TDATA,
    input [((C_S_AXIS_DATA_WIDTH/8))-1:0]S_AXIS_TSTRB,
    input [C_S_AXIS_TUSER_WIDTH-1:0] 	S_AXIS_TUSER,
    input  				S_AXIS_TVALID,
    output 				S_AXIS_TREADY,
    input  				S_AXIS_TLAST,
    input [31:0] destip_addr,
    output reg[31:0] ip_addr,
    input [5:0] cpu_hit_array
);


   integer i,j,k,l,m;
   wire [C_M_AXIS_TUSER_WIDTH-1:0] M_AXIS_TUSER0;
   wire pkt_is_from_cpu;
	
   assign pkt_is_from_cpu = M_AXIS_TUSER[SRC_PORT_POS+1] ||
			    M_AXIS_TUSER[SRC_PORT_POS+3] ||
			    M_AXIS_TUSER[SRC_PORT_POS+5] ||
			    M_AXIS_TUSER[SRC_PORT_POS+7];

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

  reg tbl_wr_ack_next,tbl_rd_ack_next;
  reg [31:0] tbl_rd_data_next;

  reg [31:0] result,dest_hit_next, bad_ttl_next, ver_next,non_ip_next, ip_data_check;
  reg header, header_next, cpu_hit, dest_ip_hit;

  always@*
  begin
     header_next = header;
     M_AXIS_TUSER = M_AXIS_TUSER0;
     if(header == 0 & M_AXIS_TVALID & !M_AXIS_TLAST) 
     begin//{
      header_next = 1;
      if(!pkt_is_from_cpu)
      begin //{
      if(cpu_hit_array != 5'd0)
      begin //{
      if(M_AXIS_TUSER0[SRC_PORT_POS])   M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00000010;
      if(M_AXIS_TUSER0[SRC_PORT_POS+2]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00001000;
      if(M_AXIS_TUSER0[SRC_PORT_POS+4]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00100000;
      if(M_AXIS_TUSER0[SRC_PORT_POS+6]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b10000000;
      end //}
      end //}
      else
      begin //{
      if(M_AXIS_TUSER0[SRC_PORT_POS+1]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00000001;
      if(M_AXIS_TUSER0[SRC_PORT_POS+3]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00000100;
      if(M_AXIS_TUSER0[SRC_PORT_POS+5]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b00010000;
      if(M_AXIS_TUSER0[SRC_PORT_POS+7]) M_AXIS_TUSER[DST_PORT_POS+7:DST_PORT_POS] = 8'b01000000;
      end //}
     end//}
   else if(header == 1 & M_AXIS_TLAST & M_AXIS_TVALID & M_AXIS_TREADY )
   begin
      header_next = 0;
   end
 end


  always@(posedge AXI_ACLK)
  begin
     ip_addr <= destip_addr;
     if(~AXI_RESETN) begin
	header <= 0;
     end
     else 
     begin
        header <= header_next;
     end
  end


endmodule

/*
	for(i = 0; i < 32; i=i+1)
	begin
	dest_ip_table_next[i] = dest_ip_table[i];
	end

 tbl_wr_ack_next = tbl_wr_ack;
	tbl_rd_ack_next = tbl_rd_ack;
	tbl_rd_data_next = tbl_rd_data;
    	if(tbl_wr_req)
    	begin
      	tbl_wr_ack_next = 1;
//      	dest_ip_table[tbl_wr_addr] = tbl_wr_data;
   	end
    	else tbl_wr_ack_next = 0; 

    	if(tbl_rd_req)
    	begin
    	tbl_rd_ack_next = 1;
      	tbl_rd_data_next = dest_ip_table[0][tbl_rd_addr];
    	end
    	else tbl_rd_ack_next = 0; 

*/
// (dest_ip_table[0],dest_ip_table[1],dest_ip_table[2],dest_ip_table[3],dest_ip_table[4],
//dest_ip_table[5],dest_ip_table[6],dest_ip_table[7],dest_ip_table[8],dest_ip_table[9],
//dest_ip_table[10],dest_ip_table[11],dest_ip_table[12],dest_ip_table[13],dest_ip_table[14],
//dest_ip_table[15],dest_ip_table[16],dest_ip_table[17],dest_ip_table[18],dest_ip_table[19],
//dest_ip_table[20],dest_ip_table[21],dest_ip_table[22],dest_ip_table[23],dest_ip_table[24],
//dest_ip_table[25],dest_ip_table[26],dest_ip_table[27],dest_ip_table[28],dest_ip_table[29],
//dest_ip_table[30],dest_ip_table[31],
//destip_addr,header,ver_count,bad_ttl_count,non_ip_count,dest_hit_count,M_AXIS_TUSER0,M_AXIS_TDATA,M_AXIS_TLAST,M_AXIS_TVALID
