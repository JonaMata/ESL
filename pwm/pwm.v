module pwm #(
    parameter DATA_WIDTH = 8,
    parameter SPEED = 20 // kHz
) (
    input wire clk,
    input wire rst,
    input wire [DATA_WIDTH-1:0] duty_cycle,
    input wire enable,
    input wire direction,
    output wire pwm_out,
    output wire ina,
    output wire inb
);
    localparam PERIOD = 50000 / SPEED; // Clock cycles per PWM period
    localparam COUNTER_PERIOD = PERIOD/(2**DATA_WIDTH-2); // Clock cycles per counter increment

    reg [$clog2(COUNTER_PERIOD)-1:0] clock_counter;
    reg [DATA_WIDTH-1:0] counter;
    reg pwm_out_reg, ina_reg, inb_reg;
    assign pwm_out = pwm_out_reg;
    assign ina = ina_reg;
    assign inb = inb_reg;

    always @(posedge clk or posedge rst) begin
        if (rst) begin
            clock_counter <= 0;
            counter <= 0;
            pwm_out_reg <= 0;
            ina_reg <= 0;
            inb_reg <= 0;
        end else begin
            if (enable) begin
                if (counter < duty_cycle) begin
                    pwm_out_reg <= 1;
                end else begin
                    pwm_out_reg <= 0;
                end

                if (direction) begin
                    ina_reg <= 1;
                    inb_reg <= 0;
                end else begin
                    ina_reg <= 0;
                    inb_reg <= 1;
                end
            end else begin
                pwm_out_reg <= 0;
                ina_reg <= 0;
                inb_reg <= 0;
            end
            clock_counter <= clock_counter + 1;
            if (clock_counter >= (COUNTER_PERIOD - 1)) begin
                clock_counter <= 0;
                if (counter >= (2**DATA_WIDTH - 2)) begin
                    counter <= 0;
                end else begin
                    counter <= counter + 1;
                end
            end
        end
    end
endmodule