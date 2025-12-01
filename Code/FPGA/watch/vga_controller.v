module vga_controller #(
    // Default Parameters for 1920x1080 @ 60Hz (Pixel Clock 148.5 MHz)
    parameter H_ACTIVE = 1920,
    parameter H_FP     = 88,
    parameter H_SYNC   = 44,
    parameter H_BP     = 148, 
    parameter V_ACTIVE = 1080,
    parameter V_FP     = 4,
    parameter V_SYNC   = 5,
    parameter V_BP     = 36
)(
    input  wire       pixel_clk,
    input  wire       rst_n,
    output wire [11:0] x,        // Current X coordinate (0-1919)
    output wire [11:0] y,        // Current Y coordinate (0-1079)
    output wire       video_on, // High when in active display area
    output reg        h_sync,
    output reg        v_sync,
    output reg        blank_n,  // DAC blanking (Active Low)
    output reg        sync_n    // DAC sync on green (Active Low)
);

    // Total sizes
    localparam H_TOTAL = H_ACTIVE + H_FP + H_SYNC + H_BP;
    localparam V_TOTAL = V_ACTIVE + V_FP + V_SYNC + V_BP;

    reg [11:0] h_cnt, v_cnt;

    // Horizontal and Vertical Counters
    always @(posedge pixel_clk or negedge rst_n) begin
        if (!rst_n) begin
            h_cnt <= 0;
            v_cnt <= 0;
        end else begin
            if (h_cnt == H_TOTAL - 1) begin
                h_cnt <= 0;
                if (v_cnt == V_TOTAL - 1)
                    v_cnt <= 0;
                else
                    v_cnt <= v_cnt + 1;
            end else begin
                h_cnt <= h_cnt + 1;
            end
        end
    end

    // Coordinate Output
    assign x = h_cnt;
    assign y = v_cnt;
    assign video_on = (h_cnt < H_ACTIVE) && (v_cnt < V_ACTIVE);

    // Sync Generation (Active High/Low depends on standard, usually Low for 1080p)
    always @(posedge pixel_clk) begin
        h_sync  <= ~((h_cnt >= H_ACTIVE + H_FP) && (h_cnt < H_ACTIVE + H_FP + H_SYNC));
        v_sync  <= ~((v_cnt >= V_ACTIVE + V_FP) && (v_cnt < V_ACTIVE + V_FP + V_SYNC));
        blank_n <= video_on; // Drive low during blanking
        sync_n  <= 1'b0;     // Usually tied low or ignored for VGA
    end

endmodule