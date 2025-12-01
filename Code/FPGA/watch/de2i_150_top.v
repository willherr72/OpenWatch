module de2i_150_top(
    input  wire        CLOCK_50,
    input  wire [3:0]  KEY,         // KEY[0] is Reset
    input  wire        UART_RXD,    // UART RX (Data In)
    output wire        UART_TXD,    // UART TX (Data Out)
    input  wire        UART_CTS,    
    output wire        UART_RTS,    // Flow Control
    
    // LCD Interface
    output wire [7:0]  LCD_DATA,
    output wire        LCD_RW,
    output wire        LCD_EN,
    output wire        LCD_RS,
    output wire        LCD_ON,

    // VGA Interface
    output wire        VGA_CLK,      // VGA Pixel Clock
    output wire        VGA_HS,       // H_SYNC
    output wire        VGA_VS,       // V_SYNC
    output wire        VGA_BLANK_N,  // Direct Blanking
    output wire        VGA_SYNC_N,   // Sync on Green
    output wire [7:0]  VGA_R,
    output wire [7:0]  VGA_G,
    output wire [7:0]  VGA_B
);

    wire rst_n = KEY[0];
    
    // Drive RTS Low (Ready to Receive)
    assign UART_RTS = 1'b0;
    // Loopback TX High (Idle)
    assign UART_TXD = 1'b1;
    
    // UART Signals
    wire [7:0] rx_data;
    wire       rx_ready;
    
    // LCD Driver Signals
    reg [7:0]  lcd_char_data;
    reg        lcd_char_valid;
    reg [1:0]  lcd_cmd_type;
    wire       lcd_busy;

    // Cursor Management
    reg [5:0]  char_count;
    reg [1:0]  fsm_state;
    
    localparam S_IDLE       = 0;
    localparam S_PRINT      = 1; // Not strictly used in simple FSM below
    localparam S_NEWLINE    = 2;
    localparam S_CLEAR      = 3;
    localparam S_WAIT_LCD   = 4;

    // ---------------------------------------------------------
    // 1. UART Receiver
    // ---------------------------------------------------------
    uart_rx #(
        .CLOCK_FREQ(50000000),
        .BAUD_RATE(9600) 
    ) uart_inst (
        .clk(CLOCK_50),
        .rst_n(rst_n),
        .rx_line(UART_RXD),
        .rx_data(rx_data),
        .rx_ready(rx_ready)
    );

    // ---------------------------------------------------------
    // 2. LCD Driver & State Machine (Restored)
    // ---------------------------------------------------------
    lcd_driver lcd_inst (
        .clk(CLOCK_50),
        .rst_n(rst_n),
        .char_data(lcd_char_data),
        .char_valid(lcd_char_valid),
        .cmd_type(lcd_cmd_type),
        .busy(lcd_busy),
        .LCD_DATA(LCD_DATA),
        .LCD_RW(LCD_RW),
        .LCD_EN(LCD_EN),
        .LCD_RS(LCD_RS),
        .LCD_ON(LCD_ON)
    );

    // LCD Main Logic Controller
    always @(posedge CLOCK_50 or negedge rst_n) begin
        if (!rst_n) begin
            char_count <= 0;
            lcd_char_valid <= 0;
            fsm_state <= S_IDLE;
            lcd_char_data <= 0;
            lcd_cmd_type <= 0;
        end else begin
            // Default valid to 0
            lcd_char_valid <= 0;

            case (fsm_state)
                S_IDLE: begin
                    if (rx_ready && !lcd_busy) begin
                        // If we receive a byte from UART
                        lcd_char_data <= rx_data;
                        lcd_cmd_type  <= 2'b00; // Write Char
                        lcd_char_valid <= 1;
                        fsm_state <= S_WAIT_LCD;
                    end
                end

                S_WAIT_LCD: begin
                    // Wait for LCD to accept command and finish
                    if (!lcd_busy) begin
                        char_count <= char_count + 1;
                        // Logic to wrap lines: 0-15: Line 1, 16-31: Line 2
                        if (char_count == 15) begin
                             fsm_state <= S_NEWLINE;
                        end else if (char_count == 31) begin
                            fsm_state <= S_CLEAR;
                        end else begin
                            fsm_state <= S_IDLE;
                        end
                    end
                end

                S_NEWLINE: begin
                    if (!lcd_busy) begin
                        lcd_cmd_type <= 2'b10; // Command: Goto Line 2
                        lcd_char_valid <= 1;
                        fsm_state <= S_IDLE;
                    end
                end

                S_CLEAR: begin
                     if (!lcd_busy) begin
                        lcd_cmd_type <= 2'b11; // Command: Clear
                        lcd_char_valid <= 1;
                        char_count <= 0;
                        fsm_state <= S_IDLE;
                    end
                end
            endcase
        end
    end

    // ---------------------------------------------------------
    // 3. VGA Subsystem
    // ---------------------------------------------------------
    wire pixel_clk; 
    wire pll_locked;
    
    // Instantiating the PLL generated in Step 1
    vga_pll pll_inst (
        .inclk0(CLOCK_50),
        .c0(pixel_clk),
        .locked(pll_locked)
    );
    
    // VGA Controller
    wire [11:0] vga_x, vga_y;
    wire video_active;
    
    vga_controller vga_c (
        .pixel_clk(pixel_clk),
        .rst_n(rst_n & pll_locked),
        .x(vga_x),
        .y(vga_y),
        .video_on(video_active),
        .h_sync(VGA_HS),
        .v_sync(VGA_VS),
        .blank_n(VGA_BLANK_N),
        .sync_n(VGA_SYNC_N)
    );
    
    assign VGA_CLK = pixel_clk; // Output clock to DAC
    
    // Text Generator
    text_screen_gen text_gen (
        .clk(CLOCK_50),
        .pixel_clk(pixel_clk),
        .rst_n(rst_n),
        .x(vga_x),
        .y(vga_y),
        .video_on(video_active),
        .uart_data(rx_data),
        .uart_valid(rx_ready), // Share the valid pulse with VGA
        .vga_r(VGA_R),
        .vga_g(VGA_G),
        .vga_b(VGA_B)
    );

endmodule