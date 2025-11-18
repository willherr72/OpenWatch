#!/usr/bin/env python3
"""
Serial Port Forwarder with LCD Formatting
Reads sensor data from COM15, formats it for 16x2 LCD, and sends to COM5
"""

import serial
import sys
import time
import re


def clean_special_chars(text):
    """Remove special characters like degree symbols, mu, etc."""
    text = text.replace('°', '')
    text = text.replace('µ', 'u')
    text = text.replace('²', '2')
    return text


def parse_bmp280(line):
    """Parse BMP280 sensor line and format for LCD"""
    # BMP280: Temp: 35.48°C, Pressure: 1013.75 hPa
    temp_match = re.search(r'Temp:\s*([\d.]+)', line)
    pressure_match = re.search(r'Pressure:\s*([\d.]+)', line)
    
    if temp_match and pressure_match:
        temp = float(temp_match.group(1))
        pressure = float(pressure_match.group(1))
        return [
            f"Temp: {temp:.1f}C",
            f"Press: {pressure:.0f}hPa"
        ]
    return None


def parse_bno055_accel(line):
    """Parse BNO055 accelerometer line"""
    # Accel: X=-1.45 Y=-1.12 Z=-9.56 m/s²
    x_match = re.search(r'X=([-\d.]+)', line)
    y_match = re.search(r'Y=([-\d.]+)', line)
    z_match = re.search(r'Z=([-\d.]+)', line)
    
    if x_match and y_match and z_match:
        x = float(x_match.group(1))
        y = float(y_match.group(1))
        z = float(z_match.group(1))
        return [
            f"Accel X:{x:+.1f}",
            f"Y:{y:+.1f} Z:{z:+.1f}"
        ]
    return None


def parse_bno055_gyro(line):
    """Parse BNO055 gyroscope line"""
    # Gyro:  X=0.00 Y=-0.12 Z=0.19 rad/s
    x_match = re.search(r'X=([-\d.]+)', line)
    y_match = re.search(r'Y=([-\d.]+)', line)
    z_match = re.search(r'Z=([-\d.]+)', line)
    
    if x_match and y_match and z_match:
        x = float(x_match.group(1))
        y = float(y_match.group(1))
        z = float(z_match.group(1))
        return [
            f"Gyro X:{x:+.2f}",
            f"Y:{y:+.2f} Z:{z:+.2f}"
        ]
    return None


def parse_bno055_mag(line):
    """Parse BNO055 magnetometer line"""
    # Mag:   X=-247.25 Y=6.75 Z=13.00 µT
    x_match = re.search(r'X=([-\d.]+)', line)
    y_match = re.search(r'Y=([-\d.]+)', line)
    z_match = re.search(r'Z=([-\d.]+)', line)
    
    if x_match and y_match and z_match:
        x = float(x_match.group(1))
        y = float(y_match.group(1))
        z = float(z_match.group(1))
        return [
            f"Mag X:{x:+.0f}uT",
            f"Y:{y:+.0f} Z:{z:+.0f}"
        ]
    return None


def parse_bno055_orient(line):
    """Parse BNO055 orientation line"""
    # Orient: Pitch=-8.38° (inc), Yaw=162.75° (azm), Roll=175.19° (toolface)
    pitch_match = re.search(r'Pitch=([-\d.]+)', line)
    yaw_match = re.search(r'Yaw=([-\d.]+)', line)
    roll_match = re.search(r'Roll=([-\d.]+)', line)
    
    if pitch_match and yaw_match and roll_match:
        pitch = float(pitch_match.group(1))
        yaw = float(yaw_match.group(1))
        roll = float(roll_match.group(1))
        return [
            f"Pitch:{pitch:+.1f}",
            f"Yaw:{yaw:.0f} R:{roll:.0f}"
        ]
    return None


def parse_sensor_data(buffer):
    """Parse sensor data from buffer and extract formatted LCD lines"""
    lcd_data = []
    lines = buffer.split('\n')
    
    for i, line in enumerate(lines):
        line = line.strip()
        
        # BMP280 sensor
        if 'BMP280:' in line:
            result = parse_bmp280(line)
            if result:
                lcd_data.append(('BMP280', result))
        
        # BNO055 Accelerometer
        elif 'Accel:' in line:
            result = parse_bno055_accel(line)
            if result:
                lcd_data.append(('BNO055_ACCEL', result))
        
        # BNO055 Gyroscope
        elif 'Gyro:' in line:
            result = parse_bno055_gyro(line)
            if result:
                lcd_data.append(('BNO055_GYRO', result))
        
        # BNO055 Magnetometer
        elif 'Mag:' in line:
            result = parse_bno055_mag(line)
            if result:
                lcd_data.append(('BNO055_MAG', result))
        
        # BNO055 Orientation
        elif 'Orient:' in line:
            result = parse_bno055_orient(line)
            if result:
                lcd_data.append(('BNO055_ORIENT', result))
    
    return lcd_data


def send_to_lcd(output_serial, line1, line2):
    """Send two lines to LCD via serial port"""
    # FPGA treats display as 40x2, so pad each line to 40 chars
    # Physical display is 16x2, so only first 16 chars of each line are visible
    line1 = line1[:16].ljust(40)  # Pad to 40 chars (16 visible + 24 blank)
    line2 = line2[:16].ljust(40)  # Pad to 40 chars (16 visible + 24 blank)
    
    # Send both lines
    output_serial.write(line1.encode('ascii'))
    output_serial.write(line2.encode('ascii'))
    output_serial.flush()


def main():
    # Serial port configuration
    INPUT_PORT = 'COM15'
    OUTPUT_PORT = 'COM5'
    INPUT_BAUD = 115200  # Baud rate for sensor input
    OUTPUT_BAUD = 9600   # Baud rate for LCD output (adjust as needed)
    TIMEOUT = 0.1  # Read timeout in seconds
    DISPLAY_DELAY = 1.0  # Delay between LCD updates in seconds
    
    input_serial = None
    output_serial = None
    
    try:
        # Open input serial port (COM15)
        print(f"Opening {INPUT_PORT} for reading...")
        input_serial = serial.Serial(
            port=INPUT_PORT,
            baudrate=INPUT_BAUD,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=TIMEOUT
        )
        print(f"✓ {INPUT_PORT} opened successfully")
        
        # Open output serial port (COM5)
        print(f"Opening {OUTPUT_PORT} for writing...")
        output_serial = serial.Serial(
            port=OUTPUT_PORT,
            baudrate=OUTPUT_BAUD,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=TIMEOUT
        )
        print(f"✓ {OUTPUT_PORT} opened successfully")
        
        print("\nParsing sensor data and sending to LCD...")
        print("Press Ctrl+C to stop\n")
        
        data_buffer = ""
        screens_sent = 0
        
        # Main loop
        while True:
            # Read data from input port and accumulate in buffer
            if input_serial.in_waiting > 0:
                new_data = input_serial.read(input_serial.in_waiting).decode('ascii', errors='ignore')
                data_buffer += new_data
                
                # Look for complete sensor data block (between === markers)
                if '=== SENSOR DATA ===' in data_buffer and '==================' in data_buffer:
                    # Extract the sensor data block
                    start_idx = data_buffer.find('=== SENSOR DATA ===')
                    end_idx = data_buffer.find('==================', start_idx)
                    
                    if end_idx > start_idx:
                        sensor_block = data_buffer[start_idx:end_idx + 18]
                        
                        # Parse the sensor data
                        lcd_data = parse_sensor_data(sensor_block)
                        
                        # Send each sensor's data to LCD with delays
                        if lcd_data:
                            print(f"\n--- Sending {len(lcd_data)} screens to LCD ---")
                            for sensor_name, lines in lcd_data:
                                print(f"  [{sensor_name}]")
                                print(f"    Line 1: {lines[0]}")
                                print(f"    Line 2: {lines[1]}")
                                
                                send_to_lcd(output_serial, lines[0], lines[1])
                                screens_sent += 1
                                
                                # Wait before sending next screen
                                time.sleep(DISPLAY_DELAY)
                        
                        # Clear processed data from buffer
                        data_buffer = data_buffer[end_idx + 18:]
            
            # Small delay to prevent CPU spinning
            time.sleep(0.01)
    
    except serial.SerialException as e:
        print(f"\n❌ Serial port error: {e}", file=sys.stderr)
        return 1
    
    except KeyboardInterrupt:
        print("\n\n✓ Program stopped by user")
        print(f"Total screens sent: {screens_sent}")
        return 0
    
    except Exception as e:
        print(f"\n❌ Unexpected error: {e}", file=sys.stderr)
        return 1
    
    finally:
        # Clean up - close serial ports
        if input_serial and input_serial.is_open:
            input_serial.close()
            print(f"Closed {INPUT_PORT}")
        
        if output_serial and output_serial.is_open:
            output_serial.close()
            print(f"Closed {OUTPUT_PORT}")


if __name__ == "__main__":
    sys.exit(main())

