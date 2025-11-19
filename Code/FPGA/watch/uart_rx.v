module uart_rx #(
    parameter CLOCK_FREQ = 50000000, // 50 MHz
    parameter BAUD_RATE  = 9600
)(
    input  wire       clk,
    input  wire       rst_n,
    input  wire       rx_line,
    output reg  [7:0] rx_data,
    output reg        rx_ready // Pulse one clock cycle when data is valid
);

    localparam CLKS_PER_BIT = CLOCK_FREQ / BAUD_RATE;
    
    // States
    localparam S_IDLE       = 3'b000;
    localparam S_START_BIT  = 3'b001;
    localparam S_DATA_BITS  = 3'b010;
    localparam S_STOP_BIT   = 3'b011;
    localparam S_CLEANUP    = 3'b100;

    reg [2:0] state     = S_IDLE;
    reg [15:0] clk_cnt  = 0;
    reg [2:0] bit_index = 0;
    reg [7:0] rx_byte   = 0;

    // Double buffer incoming data to synchronize to clock domain and avoid metastability
    reg rx_sync_stage1 = 1;
    reg rx_sync_stage2 = 1;

    always @(posedge clk) begin
        rx_sync_stage1 <= rx_line;
        rx_sync_stage2 <= rx_sync_stage1;
    end
    
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state       <= S_IDLE;
            clk_cnt     <= 0;
            bit_index   <= 0;
            rx_data     <= 0;
            rx_ready    <= 0;
        end else begin
            rx_ready <= 0; // Default low

            case (state)
                S_IDLE: begin
                    clk_cnt <= 0;
                    bit_index <= 0;
                    if (rx_sync_stage2 == 1'b0) begin // Start bit detected
                        state <= S_START_BIT;
                    end
                end

                // Check middle of start bit to ensure it's still low
                S_START_BIT: begin
                    if (clk_cnt == (CLKS_PER_BIT - 1) / 2) begin
                        if (rx_sync_stage2 == 1'b0) begin
                            clk_cnt <= 0;
                            state   <= S_DATA_BITS;
                        end else begin
                            state   <= S_IDLE;
                        end
                    end else begin
                        clk_cnt <= clk_cnt + 1;
                    end
                end

                // Sample data bits
                S_DATA_BITS: begin
                    if (clk_cnt < CLKS_PER_BIT - 1) begin
                        clk_cnt <= clk_cnt + 1;
                    end else begin
                        clk_cnt <= 0;
                        rx_byte[bit_index] <= rx_sync_stage2;
                        
                        if (bit_index < 7) begin
                            bit_index <= bit_index + 1;
                        end else begin
                            bit_index <= 0;
                            state     <= S_STOP_BIT;
                        end
                    end
                end

                // Stop bit
                S_STOP_BIT: begin
                    if (clk_cnt < CLKS_PER_BIT - 1) begin
                        clk_cnt <= clk_cnt + 1;
                    end else begin
                        rx_ready <= 1'b1;
                        rx_data  <= rx_byte;
                        clk_cnt  <= 0;
                        state    <= S_CLEANUP;
                    end
                end

                S_CLEANUP: begin
                    state <= S_IDLE;
                end
                
                default: state <= S_IDLE;
            endcase
        end
    end
endmodule