module font_rom (
    input  wire [6:0] char_code, 
    input  wire [3:0] row,       
    output reg  [7:0] row_pixels 
);
    always @(*) begin
        case (char_code)
            // --- Symbols ---
            7'h20: row_pixels = 8'h00; // Space
            7'h2B: begin // +
                case(row) 4'h4:row_pixels=8'h18; 4'h5:row_pixels=8'h18; 4'h6:row_pixels=8'h7E; 4'h7:row_pixels=8'h18; 4'h8:row_pixels=8'h18; default:row_pixels=8'h00; endcase end
            7'h2D: begin // -
                case(row) 4'h6:row_pixels=8'h7E; default:row_pixels=8'h00; endcase end
            7'h2E: begin // .
                case(row) 4'hC:row_pixels=8'h18; 4'hD:row_pixels=8'h18; default:row_pixels=8'h00; endcase end
            7'h3A: begin // :
                case(row) 4'h4:row_pixels=8'h18; 4'h5:row_pixels=8'h18; 4'hA:row_pixels=8'h18; 4'hB:row_pixels=8'h18; default:row_pixels=8'h00; endcase end

            // --- Numbers (0-9) ---
            7'h30: begin /* 0 */ case(row) 4'h1:row_pixels=8'h3C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h6E; 4'h4:row_pixels=8'h76; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h31: begin /* 1 */ case(row) 4'h1:row_pixels=8'h18; 4'h2:row_pixels=8'h38; 4'h3:row_pixels=8'h18; 4'h4:row_pixels=8'h18; 4'h5:row_pixels=8'h18; 4'h6:row_pixels=8'h18; 4'h7:row_pixels=8'h7E; default:row_pixels=8'h00; endcase end
            7'h32: begin /* 2 */ case(row) 4'h1:row_pixels=8'h3C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h06; 4'h4:row_pixels=8'h0C; 4'h5:row_pixels=8'h30; 4'h6:row_pixels=8'h60; 4'h7:row_pixels=8'h7E; default:row_pixels=8'h00; endcase end
            7'h33: begin /* 3 */ case(row) 4'h1:row_pixels=8'h3C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h06; 4'h4:row_pixels=8'h1C; 4'h5:row_pixels=8'h06; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h34: begin /* 4 */ case(row) 4'h1:row_pixels=8'h0C; 4'h2:row_pixels=8'h1C; 4'h3:row_pixels=8'h3C; 4'h4:row_pixels=8'h6C; 4'h5:row_pixels=8'h7E; 4'h6:row_pixels=8'h0C; 4'h7:row_pixels=8'h0C; default:row_pixels=8'h00; endcase end
            7'h35: begin /* 5 */ case(row) 4'h1:row_pixels=8'h7E; 4'h2:row_pixels=8'h60; 4'h3:row_pixels=8'h7C; 4'h4:row_pixels=8'h06; 4'h5:row_pixels=8'h06; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h36: begin /* 6 */ case(row) 4'h1:row_pixels=8'h3C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h60; 4'h4:row_pixels=8'h7C; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h37: begin /* 7 */ case(row) 4'h1:row_pixels=8'h7E; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h06; 4'h4:row_pixels=8'h0C; 4'h5:row_pixels=8'h18; 4'h6:row_pixels=8'h18; 4'h7:row_pixels=8'h18; default:row_pixels=8'h00; endcase end
            7'h38: begin /* 8 */ case(row) 4'h1:row_pixels=8'h3C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h3C; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h39: begin /* 9 */ case(row) 4'h1:row_pixels=8'h3C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h3E; 4'h5:row_pixels=8'h06; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end

            // --- Uppercase (A-Z) ---
            7'h41: begin /* A */ case(row) 4'h1:row_pixels=8'h3C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h7E; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h66; default:row_pixels=8'h00; endcase end
            7'h42: begin /* B */ case(row) 4'h1:row_pixels=8'h7C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h7C; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h7C; default:row_pixels=8'h00; endcase end
            7'h43: begin /* C */ case(row) 4'h1:row_pixels=8'h3C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h60; 4'h4:row_pixels=8'h60; 4'h5:row_pixels=8'h60; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h44: begin /* D */ case(row) 4'h1:row_pixels=8'h78; 4'h2:row_pixels=8'h6C; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h6C; 4'h7:row_pixels=8'h78; default:row_pixels=8'h00; endcase end
            7'h45: begin /* E */ case(row) 4'h1:row_pixels=8'h7E; 4'h2:row_pixels=8'h60; 4'h3:row_pixels=8'h60; 4'h4:row_pixels=8'h78; 4'h5:row_pixels=8'h60; 4'h6:row_pixels=8'h60; 4'h7:row_pixels=8'h7E; default:row_pixels=8'h00; endcase end
            7'h46: begin /* F */ case(row) 4'h1:row_pixels=8'h7E; 4'h2:row_pixels=8'h60; 4'h3:row_pixels=8'h60; 4'h4:row_pixels=8'h78; 4'h5:row_pixels=8'h60; 4'h6:row_pixels=8'h60; 4'h7:row_pixels=8'h60; default:row_pixels=8'h00; endcase end
            7'h47: begin /* G */ case(row) 4'h1:row_pixels=8'h3C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h60; 4'h4:row_pixels=8'h6E; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h48: begin /* H */ case(row) 4'h1:row_pixels=8'h66; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h7E; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h66; default:row_pixels=8'h00; endcase end
            7'h49: begin /* I */ case(row) 4'h1:row_pixels=8'h3C; 4'h2:row_pixels=8'h18; 4'h3:row_pixels=8'h18; 4'h4:row_pixels=8'h18; 4'h5:row_pixels=8'h18; 4'h6:row_pixels=8'h18; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h4A: begin /* J */ case(row) 4'h1:row_pixels=8'h1E; 4'h2:row_pixels=8'h0C; 4'h3:row_pixels=8'h0C; 4'h4:row_pixels=8'h0C; 4'h5:row_pixels=8'h0C; 4'h6:row_pixels=8'h6C; 4'h7:row_pixels=8'h38; default:row_pixels=8'h00; endcase end
            7'h4B: begin /* K */ case(row) 4'h1:row_pixels=8'h66; 4'h2:row_pixels=8'h6C; 4'h3:row_pixels=8'h78; 4'h4:row_pixels=8'h70; 4'h5:row_pixels=8'h78; 4'h6:row_pixels=8'h6C; 4'h7:row_pixels=8'h66; default:row_pixels=8'h00; endcase end
            7'h4C: begin /* L */ case(row) 4'h1:row_pixels=8'h60; 4'h2:row_pixels=8'h60; 4'h3:row_pixels=8'h60; 4'h4:row_pixels=8'h60; 4'h5:row_pixels=8'h60; 4'h6:row_pixels=8'h60; 4'h7:row_pixels=8'h7E; default:row_pixels=8'h00; endcase end
            7'h4D: begin /* M */ case(row) 4'h1:row_pixels=8'h66; 4'h2:row_pixels=8'h7E; 4'h3:row_pixels=8'h7E; 4'h4:row_pixels=8'h7E; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h66; default:row_pixels=8'h00; endcase end
            7'h4E: begin /* N */ case(row) 4'h1:row_pixels=8'h66; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h76; 4'h4:row_pixels=8'h7E; 4'h5:row_pixels=8'h6E; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h66; default:row_pixels=8'h00; endcase end
            7'h4F: begin /* O */ case(row) 4'h1:row_pixels=8'h3C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h50: begin /* P */ case(row) 4'h1:row_pixels=8'h7C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h7C; 4'h5:row_pixels=8'h60; 4'h6:row_pixels=8'h60; 4'h7:row_pixels=8'h60; default:row_pixels=8'h00; endcase end
            7'h51: begin /* Q */ case(row) 4'h1:row_pixels=8'h3C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h3C; 4'h7:row_pixels=8'h0E; default:row_pixels=8'h00; endcase end
            7'h52: begin /* R */ case(row) 4'h1:row_pixels=8'h7C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h7C; 4'h5:row_pixels=8'h78; 4'h6:row_pixels=8'h6C; 4'h7:row_pixels=8'h66; default:row_pixels=8'h00; endcase end
            7'h53: begin /* S */ case(row) 4'h1:row_pixels=8'h3C; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h60; 4'h4:row_pixels=8'h3C; 4'h5:row_pixels=8'h06; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h54: begin /* T */ case(row) 4'h1:row_pixels=8'h7E; 4'h2:row_pixels=8'h18; 4'h3:row_pixels=8'h18; 4'h4:row_pixels=8'h18; 4'h5:row_pixels=8'h18; 4'h6:row_pixels=8'h18; 4'h7:row_pixels=8'h18; default:row_pixels=8'h00; endcase end
            7'h55: begin /* U */ case(row) 4'h1:row_pixels=8'h66; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h56: begin /* V */ case(row) 4'h1:row_pixels=8'h66; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h3C; 4'h7:row_pixels=8'h18; default:row_pixels=8'h00; endcase end
            7'h57: begin /* W */ case(row) 4'h1:row_pixels=8'h66; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h7E; 4'h6:row_pixels=8'h7E; 4'h7:row_pixels=8'h66; default:row_pixels=8'h00; endcase end
            7'h58: begin /* X */ case(row) 4'h1:row_pixels=8'h66; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h3C; 4'h4:row_pixels=8'h18; 4'h5:row_pixels=8'h3C; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h66; default:row_pixels=8'h00; endcase end
            7'h59: begin /* Y */ case(row) 4'h1:row_pixels=8'h66; 4'h2:row_pixels=8'h66; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h3C; 4'h5:row_pixels=8'h18; 4'h6:row_pixels=8'h18; 4'h7:row_pixels=8'h18; default:row_pixels=8'h00; endcase end
            7'h5A: begin /* Z */ case(row) 4'h1:row_pixels=8'h7E; 4'h2:row_pixels=8'h06; 4'h3:row_pixels=8'h0C; 4'h4:row_pixels=8'h18; 4'h5:row_pixels=8'h30; 4'h6:row_pixels=8'h60; 4'h7:row_pixels=8'h7E; default:row_pixels=8'h00; endcase end

            // --- Lowercase (a-z) ---
            7'h61: begin /* a */ case(row) 4'h3:row_pixels=8'h3C; 4'h4:row_pixels=8'h06; 4'h5:row_pixels=8'h3E; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3E; default:row_pixels=8'h00; endcase end
            7'h62: begin /* b */ case(row) 4'h1:row_pixels=8'h60; 4'h2:row_pixels=8'h60; 4'h3:row_pixels=8'h7C; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h7C; default:row_pixels=8'h00; endcase end
            7'h63: begin /* c */ case(row) 4'h3:row_pixels=8'h3C; 4'h4:row_pixels=8'h60; 4'h5:row_pixels=8'h60; 4'h6:row_pixels=8'h60; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h64: begin /* d */ case(row) 4'h1:row_pixels=8'h06; 4'h2:row_pixels=8'h06; 4'h3:row_pixels=8'h3E; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3E; default:row_pixels=8'h00; endcase end
            7'h65: begin /* e */ case(row) 4'h3:row_pixels=8'h3C; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h7E; 4'h6:row_pixels=8'h60; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h66: begin /* f */ case(row) 4'h1:row_pixels=8'h0C; 4'h2:row_pixels=8'h18; 4'h3:row_pixels=8'h7E; 4'h4:row_pixels=8'h18; 4'h5:row_pixels=8'h18; 4'h6:row_pixels=8'h18; 4'h7:row_pixels=8'h18; default:row_pixels=8'h00; endcase end
            7'h67: begin /* g */ case(row) 4'h3:row_pixels=8'h3E; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h3E; 4'h7:row_pixels=8'h06; 4'h8:row_pixels=8'h7C; default:row_pixels=8'h00; endcase end
            7'h68: begin /* h */ case(row) 4'h1:row_pixels=8'h60; 4'h2:row_pixels=8'h60; 4'h3:row_pixels=8'h6C; 4'h4:row_pixels=8'h76; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h66; default:row_pixels=8'h00; endcase end
            7'h69: begin /* i */ case(row) 4'h1:row_pixels=8'h18; 4'h2:row_pixels=8'h00; 4'h3:row_pixels=8'h38; 4'h4:row_pixels=8'h18; 4'h5:row_pixels=8'h18; 4'h6:row_pixels=8'h18; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h6A: begin /* j */ case(row) 4'h1:row_pixels=8'h06; 4'h2:row_pixels=8'h00; 4'h3:row_pixels=8'h0E; 4'h4:row_pixels=8'h06; 4'h5:row_pixels=8'h06; 4'h6:row_pixels=8'h06; 4'h7:row_pixels=8'h66; 4'h8:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h6B: begin /* k */ case(row) 4'h1:row_pixels=8'h60; 4'h2:row_pixels=8'h60; 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h6C; 4'h5:row_pixels=8'h78; 4'h6:row_pixels=8'h6C; 4'h7:row_pixels=8'h66; default:row_pixels=8'h00; endcase end
            7'h6C: begin /* l */ case(row) 4'h1:row_pixels=8'h18; 4'h2:row_pixels=8'h18; 4'h3:row_pixels=8'h18; 4'h4:row_pixels=8'h18; 4'h5:row_pixels=8'h18; 4'h6:row_pixels=8'h18; 4'h7:row_pixels=8'h0C; default:row_pixels=8'h00; endcase end
            7'h6D: begin /* m */ case(row) 4'h3:row_pixels=8'hEC; 4'h4:row_pixels=8'hFE; 4'h5:row_pixels=8'hD6; 4'h6:row_pixels=8'hD6; 4'h7:row_pixels=8'hC6; default:row_pixels=8'h00; endcase end
            7'h6E: begin /* n */ case(row) 4'h3:row_pixels=8'h6C; 4'h4:row_pixels=8'h76; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h66; default:row_pixels=8'h00; endcase end
            7'h6F: begin /* o */ case(row) 4'h3:row_pixels=8'h3C; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h70: begin /* p */ case(row) 4'h3:row_pixels=8'h6C; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h7C; 4'h7:row_pixels=8'h60; 4'h8:row_pixels=8'h60; default:row_pixels=8'h00; endcase end
            7'h71: begin /* q */ case(row) 4'h3:row_pixels=8'h36; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h3E; 4'h7:row_pixels=8'h06; 4'h8:row_pixels=8'h06; default:row_pixels=8'h00; endcase end
            7'h72: begin /* r */ case(row) 4'h3:row_pixels=8'h6E; 4'h4:row_pixels=8'h72; 4'h5:row_pixels=8'h60; 4'h6:row_pixels=8'h60; 4'h7:row_pixels=8'h60; default:row_pixels=8'h00; endcase end
            7'h73: begin /* s */ case(row) 4'h3:row_pixels=8'h3C; 4'h4:row_pixels=8'h60; 4'h5:row_pixels=8'h3C; 4'h6:row_pixels=8'h06; 4'h7:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h74: begin /* t */ case(row) 4'h2:row_pixels=8'h10; 4'h3:row_pixels=8'h7E; 4'h4:row_pixels=8'h10; 4'h5:row_pixels=8'h10; 4'h6:row_pixels=8'h10; 4'h7:row_pixels=8'h0E; default:row_pixels=8'h00; endcase end
            7'h75: begin /* u */ case(row) 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h66; 4'h7:row_pixels=8'h3E; default:row_pixels=8'h00; endcase end
            7'h76: begin /* v */ case(row) 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h3C; 4'h7:row_pixels=8'h18; default:row_pixels=8'h00; endcase end
            7'h77: begin /* w */ case(row) 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h7E; 4'h6:row_pixels=8'h7E; 4'h7:row_pixels=8'h66; default:row_pixels=8'h00; endcase end
            7'h78: begin /* x */ case(row) 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h3C; 4'h5:row_pixels=8'h18; 4'h6:row_pixels=8'h3C; 4'h7:row_pixels=8'h66; default:row_pixels=8'h00; endcase end
            7'h79: begin /* y */ case(row) 4'h3:row_pixels=8'h66; 4'h4:row_pixels=8'h66; 4'h5:row_pixels=8'h66; 4'h6:row_pixels=8'h3E; 4'h7:row_pixels=8'h06; 4'h8:row_pixels=8'h3C; default:row_pixels=8'h00; endcase end
            7'h7A: begin /* z */ case(row) 4'h3:row_pixels=8'h7E; 4'h4:row_pixels=8'h0C; 4'h5:row_pixels=8'h18; 4'h6:row_pixels=8'h30; 4'h7:row_pixels=8'h7E; default:row_pixels=8'h00; endcase end

            default: row_pixels = 8'h00; // Black for undefined
        endcase
    end
endmodule