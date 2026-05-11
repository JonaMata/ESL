`timescale 1ps / 1ps
module jiwy #(
    parameter DATA_WIDTH = 32,
    parameter PWM_WIDTH = 8,
    parameter PWM_KHZ = 20
) (
    input wire [7:0] slave_address,
    input wire slave_read,
    output reg [DATA_WIDTH-1:0] slave_readdata,
    input wire slave_write,
    input wire [DATA_WIDTH-1:0] slave_writedata,
    input wire clk,
    input wire reset,
    input wire [(DATA_WIDTH/8)-1:0] slave_byteenable,
    output wire [2:0] motor_yaw,
    output wire [2:0] motor_pitch,
    input wire [1:0] enc_yaw,
    input wire [1:0] enc_pitch,
);

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
        .count(yaw_enc_count)
        .count_reset(yaw_count_reset)
    );


    encoder #(
        .DATA_WIDTH(DATA_WIDTH/2)
    ) pitch_encoder (
        .clk(clk),
        .rst(reset),
        .a(enc_pitch[0]),
        .b(enc_pitch[1]),
        .count(pitch_enc_count)
        .count_reset(pitch_count_reset)
    );

    always @(posedge clk or posedge reset) begin
        if (reset) begin
            slave_readdata <= 0;
        end else begin
            if (slave_read) begin
                slave_readdata <= {yaw_enc_count, pitch_enc_count};
            end
            if (slave_write) begin
                in_mem <= slave_writedata;
            end;
        end
    end
endmodule