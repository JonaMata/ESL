// TopEntity.v
// Contains a verilog module called TopEntity that inplements a simple SPI bouncer.
// What it receives in transaction N, it will send back in transaction N+1.
// Look into SPI and Full-Duplex connection for more information if this is unclear
//
// Heavily insired by https://www.fpga4fun.com/SPI2.html
// Code is intentionally left uncommented as it is only to demonstrate using the Logic Analyzer for SPI readout,
// not necessarely a "how-to" on verilog SPI inplementation.
`timescale 1ps / 1ps

module jiwy_icoboard #(
    parameter DATA_WIDTH = 32,
    parameter PWM_WIDTH = 8,
    parameter PWM_KHZ = 20
)(
    input  clk,
    input  reset,
    input  SPI_CLK,
    input  SPI_PICO,
    input  SPI_CS,
    input [1:0] enc_yaw,
    input [1:0] enc_pitch,
    output SPI_POCI,
    output [3:0] motor_pitch,
    output [3:0] motor_yaw
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

  // Receiving data -----------------
  reg [4:0] bitcnt;
  reg received_data_bool;
  reg [31:0] data_received;
  reg [7:0] yaw_duty_cycle

  always @(posedge clk) begin
    if (~SPI_CS_active) bitcnt <= 5'b00000;
    else if (SPI_CLK_risingedge) begin
      bitcnt <= bitcnt + 5'b00000;
      data_received <= {data_received[31:0], SPI_PICO_data};
    end
  end

  always @(posedge clk) received_data_bool <= SPI_CS_active && SPI_CLK_risingedge && (bitcnt == 5'b11111);

  

  always @(posedge clk) if (received_data_bool) begin
    // Process incoming 32 bits here

  end

 // Sending data ------------------
  reg [31:0] data_sent;
  reg [7:0] cnt;
  always @(posedge clk) if (SPI_CS_startmessage) cnt <= cnt + 8'h1;


  always @(posedge clk)
    if (SPI_CS_active) begin
      if (SPI_CS_startmessage) data_sent <= {yaw_enc_count, pitch_enc_count};
      else if (SPI_CLK_fallingedge) begin
        if (bitcnt == 5'b00000) data_sent <= 8'h00;
        else data_sent <= {data_sent[30:0], 1'b0};
      end
    end

  assign SPI_POCI = data_sent[7];

  // Generating PWM & reading encoders
    reg [31:0] in_mem;

    wire yaw_count_reset;
    wire pitch_count_reset;

    wire [7:0] yaw_duty_cycle;
    wire yaw_enable;
    wire yaw_direction;

    wire [7:0] pitch_duty_cycle;
    wire pitch_enable;
    wire pitch_direction;

    assign yaw_count_reset = in_mem[30];
    assign pitch_count_reset = in_mem[31];

    assign yaw_duty_cycle = in_mem[PWM_WIDTH-1:0];
    assign yaw_enable = in_mem[PWM_WIDTH];
    assign yaw_direction = in_mem[PWM_WIDTH+1];

    assign pitch_duty_cycle = in_mem[2*PWM_WIDTH-1:PWM_WIDTH];
    assign pitch_enable = in_mem[2*PWM_WIDTH];
    assign pitch_direction = in_mem[2*PWM_WIDTH+1];

    wire [15:0] yaw_enc_count;
    wire [15:0] pitch_enc_count;

    pwm #(
        .DATA_WIDTH(PWM_WIDTH),
        .SPEED(PWM_KHZ)
    ) yaw_pwm (
        .clk(clk),
        .rst(reset),
        .duty_cycle(yaw_duty_cycle),
        .enable(yaw_enable),
        .direction(yaw_direction),
        .pwm_out(motor_yaw[0]),
        .ina(motor_yaw[1]),
        .inb(motor_yaw[2])
    );

    pwm #(
        .DATA_WIDTH(PWM_WIDTH),
        .SPEED(PWM_KHZ)
    ) pitch_pwm (
        .clk(clk),
        .rst(reset),
        .duty_cycle(pitch_duty_cycle),
        .enable(pitch_enable),
        .direction(pitch_direction),
        .pwm_out(motor_pitch[0]),
        .ina(motor_pitch[1]),
        .inb(motor_pitch[2])
    );


    encoder #(
        .DATA_WIDTH(DATA_WIDTH/2)
    ) yaw_encoder (
        .clk(clk),
        .rst(reset),
        .a(enc_yaw[0]),
        .b(enc_yaw[1]),
        .count(yaw_enc_count),
        .count_reset(yaw_count_reset)
    );


    encoder #(
        .DATA_WIDTH(DATA_WIDTH/2)
    ) pitch_encoder (
        .clk(clk),
        .rst(reset),
        .a(enc_pitch[0]),
        .b(enc_pitch[1]),
        .count(pitch_enc_count),
        .count_reset(pitch_count_reset)
    );

endmodule
