module lcd_driver(
    input  wire        clk,          // 50MHz clock
    input  wire        rst_n,
    
    // User Interface
    input  wire [7:0]  char_data,    // Data to write
    input  wire        char_valid,   // Pulse to initiate write
    input  wire [1:0]  cmd_type,     // 00=Write Char, 01=Line1, 10=Line2, 11=Clear
    output reg         busy,         // 1 if LCD is initializing or writing
    
    // Physical Interface
    output reg [7:0]   LCD_DATA,
    output reg         LCD_RW,
    output reg         LCD_EN,
    output reg         LCD_RS,
    output reg         LCD_ON
);

    // Timing Constants for 50MHz Clock
    // 1 ms = 50,000 cycles
    localparam CNT_100US = 5000;
    localparam CNT_2MS   = 100000;
    localparam CNT_15MS  = 750000;
    
    // States
    localparam S_PWR_UP      = 0;
    localparam S_INIT_1      = 1; // Function Set 1
    localparam S_INIT_2      = 2; // Function Set 2
    localparam S_INIT_3      = 3; // Function Set 3
    localparam S_FUNC_SET    = 4; // Actual Function Set
    localparam S_DISP_OFF    = 5; 
    localparam S_DISP_CLEAR  = 6;
    localparam S_ENTRY_MODE  = 7;
    localparam S_DISP_ON     = 8;
    localparam S_IDLE        = 9;
    localparam S_WRITE_CMD   = 10;
    localparam S_WRITE_DATA  = 11;
    localparam S_WAIT_EN     = 12;
    localparam S_HOLD        = 13;
    localparam S_DELAY       = 14;

    reg [4:0]  state, next_state_after_delay;
    reg [19:0] cnt;
    reg [19:0] delay_target;
    
    // Hardwired LCD control
    initial begin
        LCD_ON = 1'b1; // Always Keep LCD Power On
        LCD_RW = 1'b0; // Always Write mode
    end

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state <= S_PWR_UP;
            cnt <= 0;
            busy <= 1;
            LCD_EN <= 0;
            LCD_RS <= 0;
            LCD_DATA <= 8'h00;
        end else begin
            
            // Default En Low
            if (state != S_WAIT_EN) LCD_EN <= 0;

            case (state)
                // --- Initialization Sequence ---
                S_PWR_UP: begin
                    busy <= 1;
                    if (cnt < CNT_15MS) cnt <= cnt + 1;
                    else begin 
                        cnt <= 0; 
                        state <= S_INIT_1; 
                    end
                end

                S_INIT_1: begin // Function Set 8-bit
                    LCD_RS <= 0; LCD_DATA <= 8'h38;
                    state <= S_WAIT_EN;
                    next_state_after_delay <= S_INIT_2;
                    delay_target <= CNT_2MS; // Wait > 4.1ms
                end
                
                S_INIT_2: begin // Function Set 8-bit
                    LCD_RS <= 0; LCD_DATA <= 8'h38;
                    state <= S_WAIT_EN;
                    next_state_after_delay <= S_INIT_3;
                    delay_target <= CNT_100US; // Wait > 100us
                end

                S_INIT_3: begin // Function Set 8-bit
                    LCD_RS <= 0; LCD_DATA <= 8'h38;
                    state <= S_WAIT_EN;
                    next_state_after_delay <= S_FUNC_SET;
                    delay_target <= CNT_100US;
                end

                S_FUNC_SET: begin // 0x38: 8-bit, 2-line, 5x7
                    LCD_RS <= 0; LCD_DATA <= 8'h38;
                    state <= S_WAIT_EN;
                    next_state_after_delay <= S_DISP_OFF;
                    delay_target <= CNT_100US;
                end

                S_DISP_OFF: begin // 0x08: Display Off
                    LCD_RS <= 0; LCD_DATA <= 8'h08;
                    state <= S_WAIT_EN;
                    next_state_after_delay <= S_DISP_CLEAR;
                    delay_target <= CNT_100US;
                end

                S_DISP_CLEAR: begin // 0x01: Clear Display
                    LCD_RS <= 0; LCD_DATA <= 8'h01;
                    state <= S_WAIT_EN;
                    next_state_after_delay <= S_ENTRY_MODE;
                    delay_target <= CNT_2MS; // Needs > 1.64ms
                end

                S_ENTRY_MODE: begin // 0x06: Inc addr, no shift
                    LCD_RS <= 0; LCD_DATA <= 8'h06;
                    state <= S_WAIT_EN;
                    next_state_after_delay <= S_DISP_ON;
                    delay_target <= CNT_100US;
                end

                S_DISP_ON: begin // 0x0C: Display On, Cursor Off, Blink Off
                    LCD_RS <= 0; LCD_DATA <= 8'h0C;
                    state <= S_WAIT_EN;
                    next_state_after_delay <= S_IDLE;
                    delay_target <= CNT_100US;
                end

                // --- Ready State ---
                S_IDLE: begin
                    busy <= 0;
                    if (char_valid) begin
                        busy <= 1;
                        case (cmd_type)
                            2'b00: begin // Write Char
                                LCD_RS <= 1; 
                                LCD_DATA <= char_data;
                                state <= S_WAIT_EN;
                                next_state_after_delay <= S_IDLE;
                                delay_target <= CNT_100US;
                            end
                            2'b01: begin // Goto Line 1 (0x80)
                                LCD_RS <= 0; 
                                LCD_DATA <= 8'h80;
                                state <= S_WAIT_EN;
                                next_state_after_delay <= S_IDLE;
                                delay_target <= CNT_100US;
                            end
                            2'b10: begin // Goto Line 2 (0xC0)
                                LCD_RS <= 0; 
                                LCD_DATA <= 8'hC0;
                                state <= S_WAIT_EN;
                                next_state_after_delay <= S_IDLE;
                                delay_target <= CNT_100US;
                            end
                            2'b11: begin // Clear Screen (0x01)
                                LCD_RS <= 0; 
                                LCD_DATA <= 8'h01;
                                state <= S_WAIT_EN;
                                next_state_after_delay <= S_IDLE;
                                delay_target <= CNT_2MS;
                            end
                        endcase
                    end
                end

                // --- Common Write States ---
                S_WAIT_EN: begin
                    // Setup time before EN goes high
                    LCD_EN <= 1;
                    state <= S_HOLD;
                    cnt <= 0;
                end

                S_HOLD: begin
                    // Hold EN high for > 450ns (50MHz * 25 = 500ns)
                    if (cnt < 25) cnt <= cnt + 1;
                    else begin
                        LCD_EN <= 0;
                        cnt <= 0;
                        state <= S_DELAY;
                    end
                end

                S_DELAY: begin
                    if (cnt < delay_target) cnt <= cnt + 1;
                    else begin
                        cnt <= 0;
                        state <= next_state_after_delay;
                    end
                end
            endcase
        end
    end

endmodule