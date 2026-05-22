`timescale 1ps / 1ps

module jiwy_icoboard #(
    parameter DATA_WIDTH = 32,
    parameter PWM_WIDTH = 8,
    parameter PWM_KHZ = 20
)(
    input  clk,
    input  btn1, //reset button
    output led1,
    output led2,
    input  SPI_CLK,
    input  SPI_PICO,
    input  SPI_CS,
    output SPI_POCI,
    input  YAW_ENC_A,
    input  YAW_ENC_B,
    input  PITCH_ENC_A,
    input  PITCH_ENC_B,
    output PITCH_DIRA,
    output PITCH_DIRB,
    output PITCH_PWM_VAL,
    output YAW_DIRA,
    output YAW_DIRB,
    output YAW_PWM_VAL
);
  // Re-assigning everything.
  wire reset;
  assign reset = btn1;

  // Setting up SPI, syncing the different parts to the FPGA clock ---
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

  // Incoming variables
  reg [31:0] in_mem;
  wire [9:0] in_yaw_mem;
  wire [9:0] in_pitch_mem;
  wire [1:0] in_enc_mem;

  assign in_yaw_mem = in_mem[9:0];
  assign in_pitch_mem = in_mem[19:10];
  assign in_enc_mem = in_mem[21:20];

  wire yaw_count_reset;
  wire pitch_count_reset;

  wire [7:0] yaw_duty_cycle;
  wire yaw_enable;
  wire yaw_direction;

  wire [7:0] pitch_duty_cycle;
  wire pitch_enable;
  wire pitch_direction;

  assign yaw_count_reset = in_enc_mem[0];
  assign pitch_count_reset = in_enc_mem[1];

  assign yaw_duty_cycle = in_yaw_mem[PWM_WIDTH-1:0];
  assign yaw_enable = in_yaw_mem[PWM_WIDTH];
  assign yaw_direction = in_yaw_mem[PWM_WIDTH+1];

  assign pitch_duty_cycle = in_pitch_mem[PWM_WIDTH-1:0];
  assign pitch_enable = in_pitch_mem[PWM_WIDTH];
  assign pitch_direction = in_pitch_mem[PWM_WIDTH+1];

  reg [1:0] packet_count = 2'b11;

  // Receiving data ------------------------------------------------
  reg [2:0] bitcnt = 3'b000;
  reg received_data_bool;
  reg [31:0] data_received;
  

  always @(posedge clk) begin
    if (~SPI_CS_active) begin
      bitcnt <= 3'b000;
      packet_count <= packet_count + 2'b01;
    end 
    else if (SPI_CLK_risingedge) begin
      bitcnt <= bitcnt + 3'b001;
      data_received <= {data_received[30:0], SPI_PICO_data};
    end
  end

  always @(posedge clk) received_data_bool <= SPI_CS_active && SPI_CLK_risingedge && packet_count == 2'b11 && (bitcnt == 3'b111);

  reg led_received = 0;
  assign led1 = led_received;

  reg led2_state = 0;
  assign led2 = led2_state;

  always @(posedge clk) if (received_data_bool) begin
    // Process incoming 32 bits here
    in_mem <= data_received;
    led_received <= ~led_received; // Toggle LED to indicate data reception
    if (data_received == 32'hFFFFFFFF) led2_state <= ~led2_state; // Toggle LED2 if data is all 1s
  end

 // Sending data ---------------------------------------------------
  reg [7:0] data_sent;
  // reg [7:0] cnt;
  wire [15:0] yaw_enc_count;
  wire [15:0] pitch_enc_count;

  // always @(posedge clk) if (SPI_CS_startmessage) cnt <= cnt + 8'h1;

  always @(posedge clk) begin
    if (SPI_CS_active) begin
      if (SPI_CS_startmessage) data_sent <= in_mem[(packet_count+1)*8-1 : packet_count*8];
      else if (SPI_CLK_fallingedge) begin
        if (bitcnt == 3'b000) data_sent <= in_mem;//{yaw_enc_count, pitch_enc_count};
        else data_sent <= {data_sent[30:0], 1'b0};
      end
    end
  end

  assign SPI_POCI = data_sent[31];

  pwm #(
      .DATA_WIDTH(PWM_WIDTH),
      .SPEED(PWM_KHZ)
  ) yaw_pwm (
      .clk(clk),
      .rst(reset),
      .duty_cycle(yaw_duty_cycle),
      .enable(yaw_enable),
      .direction(yaw_direction),
      .pwm_out(YAW_PWM_VAL),
      .ina(YAW_DIRA),
      .inb(YAW_DIRB)
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
      .pwm_out(PITCH_PWM_VAL),
      .ina(PITCH_DIRA),
      .inb(PITCH_DIRB)
  );

  encoder #(
      .DATA_WIDTH(DATA_WIDTH/2)
  ) yaw_encoder (
      .clk(clk),
      .rst(reset),
      .a(YAW_ENC_A),
      .b(YAW_ENC_B),
      .count(yaw_enc_count),
      .count_reset(yaw_count_reset)
  );

  encoder #(
      .DATA_WIDTH(DATA_WIDTH/2)
  ) pitch_encoder (
      .clk(clk),
      .rst(reset),
      .a(PITCH_ENC_A),
      .b(PITCH_ENC_B),
      .count(pitch_enc_count),
      .count_reset(pitch_count_reset)
  );

endmodule
