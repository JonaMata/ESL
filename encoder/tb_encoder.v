module tb_encoder(
    input clk,
    input rst,
    output reg correct = 0,
    output [31:0] count,
    output reg [31:0] expected_count = 0
);
    wire [31:0] count;
    reg [1:0] state = 0;


    encoder #(
        .DATA_WIDTH(32)
    ) dut (
        .clk(clk),
        .rst(rst),
        .a(state[0]),
        .b(state[1]),
        .count(count)
    );

    reg [31:0] step = 0;
    reg [31:0] new_expected_count = 0;
    // reg [1:0] speed_step = 0;

    always @(posedge clk or posedge rst) begin
        if (rst) begin
            step <= 0;
            new_expected_count <= 0;
        end else begin
            // if(speed_step == 0) begin
            if (step < 5) begin //Clockwise
                case (state)
                2'b00 : state <= 2'b01;
                2'b01 : state <= 2'b11;
                2'b11 : state <= 2'b10;
                2'b10 : state <= 2'b00;
            endcase
            new_expected_count <= new_expected_count + 1;
            step <= step + 1;
            end else if (step < 20) begin //Counter-clockwise
                case (state)
                    2'b00 : state <= 2'b10;
                    2'b10 : state <= 2'b11;
                    2'b11 : state <= 2'b01;
                    2'b01 : state <= 2'b00;
                endcase
                new_expected_count <= new_expected_count - 1;
                step <= step + 1;
            end else begin
                step <= 0;
            end
            // end
            // speed_step <= speed_step + 1;
            expected_count <= new_expected_count;
            correct <= (count == expected_count);
        end
    end
endmodule