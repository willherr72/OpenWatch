module text_screen_gen (
    input  wire        clk,          
    input  wire        pixel_clk,    
    input  wire        rst_n,
    input  wire [11:0] x,
    input  wire [11:0] y,
    input  wire        video_on,
    input  wire [7:0]  uart_data,
    input  wire        uart_valid,
    output reg  [7:0]  vga_r,
    output reg  [7:0]  vga_g,
    output reg  [7:0]  vga_b
);

    localparam COLS = 80; 
    localparam ROWS = 60; 
    
    // Text Buffer
    reg [6:0] char_ram [0:COLS*ROWS-1];
    reg [11:0] cursor_pos;
    
    // --- WRITE LOGIC (System Clock) ---
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            cursor_pos <= 0;
        end else if (uart_valid) begin
            // ----------------------------------------------------------------
            // CHANGE: Treat both \n (0x0A) and \r (0x0D) as "Next Line"
            // ----------------------------------------------------------------
            if (uart_data == 8'h0A || uart_data == 8'h0D) begin 
                // Calculate the index of the start of the next row
                // Logic: Current Row Index + 1, multiplied by columns per row
                if (cursor_pos < COLS * (ROWS - 1))
                    cursor_pos <= (cursor_pos / COLS + 1) * COLS;
                else
                    cursor_pos <= 0; // Wrap to top if at bottom of screen
            end 
            // Handle Backspace (0x08) - Optional useful feature
            else if (uart_data == 8'h08) begin 
                if (cursor_pos > 0) begin
                    cursor_pos <= cursor_pos - 1;
                    char_ram[cursor_pos - 1] <= 7'h20; // Clear char with space
                end
            end
            else begin
                // Standard Printable Character
                char_ram[cursor_pos] <= uart_data[6:0]; 
                
                if (cursor_pos < COLS*ROWS - 1)
                    cursor_pos <= cursor_pos + 1;
                else
                    cursor_pos <= 0; // Wrap to top
            end
        end
    end

    // --- READ LOGIC (Pixel Clock) ---
    wire [6:0] char_code;
    wire [3:0] char_row;
    wire [2:0] char_col;
    
    // Calculate grid (Scale 2x for readability)
    wire [11:0] grid_x = x[11:1]; 
    wire [11:0] grid_y = y[11:1]; 
    
    wire [11:0] col_idx = grid_x[11:3]; 
    wire [11:0] row_idx = grid_y[11:4]; 
    wire [11:0] read_addr = (row_idx * COLS) + col_idx;
    
    // Read from RAM
    assign char_code = (col_idx < COLS && row_idx < ROWS) ? char_ram[read_addr] : 7'h20; 
    assign char_row  = grid_y[3:0]; 
    assign char_col  = grid_x[2:0]; 
    
    // Font Lookup
    wire [7:0] font_byte;
    font_rom f_rom (
        .char_code(char_code),
        .row(char_row),
        .row_pixels(font_byte)
    );
    
    wire pixel_on = font_byte[~char_col]; 
    
    always @(posedge pixel_clk) begin
        if (video_on && pixel_on) begin
            vga_r <= 8'hFF; 
            vga_g <= 8'hFF;
            vga_b <= 8'hFF;
        end else begin
            vga_r <= 8'h00; 
            vga_g <= 8'h00;
            vga_b <= 8'h00;
        end
    end

endmodule