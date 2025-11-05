import socket
import machine
import time

# Pin and I2C setup for smartwatch (based on your pin diagram)
i2c = machine.I2C(0, scl=machine.Pin(9), sda=machine.Pin(8), freq=100000)
heart_rate_addr = 0xAE  # Writing address for MAX30102

# TCP Server setup
HOST = '127.0.0.1'  # Localhost, change if LabVIEW is on another machine
PORT = 5000         # Port to listen on (match with LabVIEW)

server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
server.bind((HOST, PORT))
server.listen(1)
print("Waiting for LabVIEW connection...")

conn, addr = server.accept()
print(f"Connected by {addr}")

def read_heart_rate():
    if i2c.is_ready(heart_rate_addr):
        data = i2c.readfrom_mem(0xAF, 0x00, 6)  # Read from MAX30102
        return int.from_bytes(data, 'big')
    return 0

try:
    while True:
        # Read heart rate from smartwatch sensor
        heart_rate = read_heart_rate()
        message = f"Heart Rate: {heart_rate}"
        
        # Send data to LabVIEW
        conn.sendall(message.encode('utf-8'))
        print(f"Sent to LabVIEW: {message}")
        
        # Receive response from LabVIEW (optional)
        data = conn.recv(1024)
        if data:
            print(f"Received from LabVIEW: {data.decode('utf-8')}")
        
        time.sleep(1)  # Adjust interval as needed

except KeyboardInterrupt:
    print("Disconnected")
    conn.close()
    server.close()