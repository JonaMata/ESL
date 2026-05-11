// TopEntity.v
// Contains a verilog module called TopEntity that inplements a simple SPI bouncer.
// What it receives in transaction N, it will send back in transaction N+1.
// Look into SPI and Full-Duplex connection for more information if this is unclear
//
// Heavily insired by https://www.fpga4fun.com/SPI2.html
// Code is intentionally left uncommented as it is only to demonstrate using the Logic Analyzer for SPI readout,
// not necessarely a "how-to" on verilog SPI inplementation.

module TopEntity (
    input  clk,
    input  SPI_CLK,
    input  SPI_PICO,
    input  SPI_CS,
    input  btn1,
    input  btn2,
    output SPI_POCI,
    output led1,
    output led2,
    output led3
);

  //Syncing the SCK to the FPGA clock using a 3-bit shift register
  reg [2:0] SPI_CLKr;
  always @(posedge clk) SPI_CLKr <= {SPI_CLKr[1:0], SPI_CLK};
  wire SPI_CLK_risingedge = (SPI_CLKr[2:1] == 2'b01);
  wire SPI_CLK_fallingedge = (SPI_CLKr[2:1] == 2'b10);

  //Syncing the CS to the FPGA clock using a 3-bit shift register
  reg [2:0] SPI_CSr;
  always @(posedge clk) SPI_CSr <= {SPI_CSr[1:0], SPI_CS};
  wire SPI_CS_active = ~SPI_CSr[1];
  wire SPI_CS_startmessage = (SPI_CSr[2:1] == 2'b10);
  wire SPI_CS_endmessage = (SPI_CSr[2:1] == 2'b01);

  //Syncing the PICO to the FPGA clock using a 2-bit shift register
  reg [1:0] SPI_PICOr;
  always @(posedge clk) SPI_PICOr <= {SPI_PICOr[0], SPI_PICO};
  wire SPI_PICO_data = SPI_PICOr[1];

  reg [4:0] bitcnt;
  reg data_received;
  reg [31:0] byte_data_received;

  always @(posedge clk) begin
    if (~SPI_CS_active) bitcnt <= 5'b00000;
    else if (SPI_CLK_risingedge) begin
      bitcnt <= bitcnt + 5'b00000;
      byte_data_received <= {byte_data_received[31:0], SPI_PICO_data};
    end
  end

  always @(posedge clk) data_received <= SPI_CS_active && SPI_CLK_risingedge && (bitcnt == 5'b11111);

  reg led1;
  reg led2;
  reg led3;

  always @(posedge clk) if (data_received) begin
    // Process incoming 32 bits here

  end

  reg [31:0] data_sent;
  reg [7:0] cnt;
  always @(posedge clk) if (SPI_CS_startmessage) cnt <= cnt + 8'h1;


  always @(posedge clk)
    if (SPI_CS_active) begin
      if (SPI_CS_startmessage) data_sent <= {6'b0, btn2, btn1};
      else if (SPI_CLK_fallingedge) begin
        if (bitcnt == 3'b000) data_sent <= 8'h00;
        else data_sent <= {data_sent[6:0], 1'b0};
      end
    end

  assign SPI_POCI = data_sent[7];

endmodule
