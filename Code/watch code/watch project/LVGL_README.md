# OpenWatch - LVGL Edition

## Overview

This smartwatch firmware has been migrated to use **LVGL (Light and Versatile Graphics Library)** for a modern, beautiful UI with smooth animations and better graphics rendering.

## Features

### Current Implementation

- ✅ **Beautiful Modern Watch Face**
  - Large, clear time display (12-hour format)
  - Circular seconds indicator arc
  - Day of week and date display
  - WiFi status indicator
  - Smooth animations
  - Dark theme with cyan accents
  - Subtle glow effects

- ✅ **LVGL Integration**
  - LVGL v9.2.2
  - Optimized for 240x240 round GC9A01A display
  - Double-buffered rendering for smooth updates
  - TFT_eSPI hardware acceleration

- ✅ **Touch Input**
  - CST816S touch controller support
  - Integrated with LVGL input system

- ✅ **Power Management**
  - Button-triggered sleep mode
  - Low power light sleep
  - Wake on button press

- ✅ **Time Sync**
  - WiFi-based NTP time synchronization
  - Automatic timezone handling (CDT/CST)
  - Visual sync status indicators

## Hardware

- **MCU**: ESP32-S3 (Adafruit QT Py ESP32-S3)
- **Display**: GC9A01A 240x240 round TFT (SPI)
- **Touch**: CST816S capacitive touch controller (I2C)
- **Buttons**: 2x physical buttons

## Pin Configuration

```
Display (SPI):
- SCLK: GPIO 12
- MOSI: GPIO 11
- MISO: GPIO 13
- CS:   GPIO 10
- DC:   GPIO 39
- RST:  GPIO 40
- BL:   GPIO 7

Touch (I2C):
- SDA:  GPIO 8
- SCL:  GPIO 9
- INT:  GPIO 41
- ADDR: 0x15

Buttons:
- Primary: GPIO 18
- Menu:    GPIO 14

Power:
- Rail: GPIO 17 (set HIGH)
```

## Building

1. Open project in PlatformIO
2. Build: `pio run`
3. Upload: `pio run -t upload`
4. Monitor: `pio device monitor`

## Usage

### Buttons

- **Primary Button**
  - Long Press: Enter sleep mode

- **Menu Button**
  - Short Press: Toggle WiFi (for time sync)

### Watch Face

The watch face displays:
- Current time (12-hour format)
- Day of week (top)
- Date (bottom)
- Circular seconds indicator
- WiFi status icon (green when connected)
- Dimmed display when time not synced

## Configuration

### WiFi Credentials

Edit `include/secrets.h`:
```cpp
#define WIFI_SSID "your-ssid"
#define WIFI_PASSWORD "your-password"
```

### LVGL Settings

Configuration in `lv_conf.h`:
- Memory: 64KB heap
- Color depth: 16-bit (RGB565)
- Buffer: 40 lines (9600 pixels per buffer)
- Theme: Dark mode enabled
- Fonts: Montserrat 12-48pt

## Architecture

```
┌─────────────────────────────────────┐
│           main.cpp                  │
│  (Main loop, WiFi, time sync)       │
└─────────────────┬───────────────────┘
                  │
        ┌─────────┴─────────┐
        │                   │
┌───────▼────────┐  ┌───────▼────────┐
│  LVGL Display  │  │  LVGL Touch    │
│   (TFT_eSPI)   │  │  (CST816S)     │
└───────┬────────┘  └───────┬────────┘
        │                   │
        └─────────┬─────────┘
                  │
        ┌─────────▼─────────┐
        │   Watch Face UI   │
        │  (LVGL widgets)   │
        └───────────────────┘
```

## Future Enhancements

- Add more watch faces (analog, digital variants)
- App menu system
- Timer/stopwatch app
- Fitness tracking
- Weather display
- Notification system
- Settings menu
- Battery level indicator

## Libraries Used

- **LVGL** (v9.2.2) - Graphics library
- **TFT_eSPI** (v2.5.43) - Display driver
- **ArduinoJson** (v7.4.2) - JSON parsing
- **CST816S** (custom) - Touch driver

## Notes

- LVGL tasks run at ~30 FPS
- Watch face updates every 1 second
- WiFi disabled by default (power saving)
- Time persists via ESP32 RTC when WiFi off
- Timezone: Central Time (UTC-5)

## Troubleshooting

### Display not working
- Check SPI pin definitions in platformio.ini
- Verify TFT_BL (backlight) is HIGH
- Ensure LVGL buffer size fits in memory

### Touch not responding
- Check I2C pins and address (0x15)
- Verify pull-up resistors on I2C lines
- Check INT pin connection

### Time not syncing
- Press Menu button to enable WiFi
- Check WiFi credentials in secrets.h
- Monitor serial output for NTP status

## License

[Your License Here]

## Author

OpenWatch Project

