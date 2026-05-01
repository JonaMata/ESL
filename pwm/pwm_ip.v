`timescale 1ps / 1ps
module pwm_ip #(
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
    output wire ina,
    output wire inb,
    output wire pwm_out
);

    reg [31:0] mem;
    wire [7:0] duty_cycle_mask;
    wire enable;
    wire direction;

    assign duty_cycle_mask = mem[PWM_WIDTH-1:0];
    assign enable = mem[PWM_WIDTH];
    assign direction = mem[PWM_WIDTH+1];

    pwm #(
        .DATA_WIDTH(PWM_WIDTH),
        .SPEED(PWM_KHZ)
    ) my_pwm (
        .clk(clk),
        .rst(reset),
        .duty_cycle(duty_cycle_mask),
        .enable(enable),
        .direction(direction),
        .pwm_out(pwm_out),
        .ina(ina),
        .inb(inb)
    );

    always @(posedge clk or posedge reset) begin
        if (reset) begin
            slave_readdata <= 0;
        end else begin
            if (slave_read) begin
                slave_readdata <= mem;
            end
            if (slave_write) begin
                mem <= slave_writedata;
            end;
        end
    end
endmodule