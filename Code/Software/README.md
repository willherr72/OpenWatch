# watch2fpga

Serial port forwarder that reads data from COM15 and sends it to COM5.

## Installation

Install UV if you haven't already:

```bash
pip install uv
```

Then install the project dependencies:

```bash
uv sync
```

## Usage

Run the program:

```bash
uv run watch2fpga.py
```

Or after installation:

```bash
uv run watch2fpga
```

The program will:
- Open COM15 for reading
- Open COM5 for writing
- Forward all data from COM15 to COM5
- Display forwarding statistics in real-time

Press `Ctrl+C` to stop the program.

## Configuration

You can modify the following parameters in `watch2fpga.py`:
- `INPUT_PORT`: Source serial port (default: COM15)
- `OUTPUT_PORT`: Destination serial port (default: COM5)
- `BAUD_RATE`: Communication speed (default: 115200)

## Requirements

- Python >= 3.8
- pyserial >= 3.5

