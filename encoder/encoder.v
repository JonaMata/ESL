module encoder #(
    parameter DATA_WIDTH = 32
) (
    input wire clk,
    input wire rst,
    input wire a,
    input wire b,
    output wire[DATA_WIDTH-1:0] count
);
    reg [1:0] prev_state;
    reg [1:0] state;
    reg [DATA_WIDTH-1:0] count_reg;

    assign count = count_reg;

    always @(posedge clk or posedge rst) begin
        if (rst) begin
            count_reg <= 0;
            state <= 2'b00;
            prev_state <= 2'b00;
        end else begin
            state <=  {a, b};
            prev_state <= state;
            case ({prev_state, state})
                4'b0001, 4'b0111, 4'b1110, 4'b1000: count_reg <= count_reg - 1;
                4'b0010, 4'b0100, 4'b1101, 4'b1011: count_reg <= count_reg + 1;
            endcase
        end
    end
endmodule
