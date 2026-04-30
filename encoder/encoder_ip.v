`timescale 1ps / 1ps
module encoder_ip #(
    parameter DATA_WIDTH = 32
) (
    input wire [7:0] slave_address,
    input wire slave_read,
    output reg [DATA_WIDTH-1:0] slave_readdata,
    input wire slave_write,
    input wire [DATA_WIDTH-1:0] slave_writedata,
    input wire clk,
    input wire reset,
    input wire [(DATA_WIDTH/8)-1:0] slave_byteenable,
    input wire a_channel,
    input wire b_channel
);

    wire [DATA_WIDTH-1:0] count;

    encoder #(
        .DATA_WIDTH(DATA_WIDTH)
    ) my_encoder (
        .clk(clk),
        .rst(reset),
        .a(a_channel),
        .b(b_channel),
        .count(count)
    );

    always @(posedge clk or posedge reset) begin
        if (reset) begin
            slave_readdata <= 0;
        end else begin
            if (slave_read) begin
                slave_readdata <= count;
            end
        end
    end
endmodule