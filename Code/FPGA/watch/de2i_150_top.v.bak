module de2i_150_top(
    input  wire        CLOCK_50,
    input  wire [3:0]  KEY,         // KEY[0] is Reset
    input  wire        UART_RXD,    // UART RX (Data In)
    output wire        UART_TXD,    // UART TX (Data Out - Optional loopback)
    input  wire        UART_CTS,    // UART CTS (Input from PC)
    output wire        UART_RTS,    // UART RTS (Output to PC - Flow Control)
    
    // LCD Interface
    output wire [7:0]  LCD_DATA,
    output wire        LCD_RW,
    output wire        LCD_EN,
    output wire        LCD_RS,
    output wire        LCD_ON
);

    wire rst_n = KEY[0];
    
    // --- FLOW CONTROL FIX ---
    // Drive RTS Low (0) to signal "Ready to Receive" to the PC.
    // The ZT3232 transceiver inverts this to +V (Asserted) on the RS-232 cable.
    assign UART_RTS = 1'b0; 
    
    // Optional: Loopback TX for testing (echoes received data back to PC)
    assign UART_TXD = 1'b1; // Idle High
    
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
    localparam S_PRINT      = 1;
    localparam S_NEWLINE    = 2;
    localparam S_CLEAR      = 3;
    localparam S_WAIT_LCD   = 4;

    // Instantiate UART Receiver
    // Default 9600 baud. 
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

    // Instantiate LCD Driver
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

    // Main Logic Controller
    always @(posedge CLOCK_50 or negedge rst_n) begin
        if (!rst_n) begin
            char_count <= 0;
            lcd_char_valid <= 0;
            fsm_state <= S_IDLE;
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
                        
                        // Logic to wrap lines
                        // 0-15: Line 1
                        // 16-31: Line 2
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
                    // Move cursor to Line 2
                    if (!lcd_busy) begin
                        lcd_cmd_type <= 2'b10; // Command: Goto Line 2
                        lcd_char_valid <= 1;
                        fsm_state <= S_IDLE; // Return to IDLE to wait for next char
                    end
                end

                S_CLEAR: begin
                    // Clear Screen and reset cursor
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

endmodule