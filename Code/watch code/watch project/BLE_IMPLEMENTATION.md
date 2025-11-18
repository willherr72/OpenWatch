# BLE Implementation Guide for OpenWatch

## Overview

OpenWatch supports **two connection modes**: WiFi (full features) and BLE (battery saver). You can switch between them in Settings.

### Why Not Run Both Simultaneously?

While ESP32-S3 technically supports WiFi/BLE coexistence, it uses [time-division multiplexing](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-guides/coexist.html) which can cause:
- Performance issues and packet delays
- Complex timing bugs
- Unpredictable behavior

**Solution**: Clean separation with mode toggle - just like Apple Watch and Samsung Galaxy Watch do it!

## Connection Modes

### 📡 WiFi Mode (Default) - Full Features
- ✅ Time sync via NTP servers
- ✅ Weather from NOAA API
- ✅ Full internet connectivity
- **Use when**: At home, office, or anywhere with WiFi
- **Power**: ~100-300mA during active use

### 🔵 BLE Mode - Battery Saver  
- ✅ Time sync from paired phone
- 🚧 Future: Notifications (when vibration motor added)
- 🚧 Future: Music control, find-my-phone
- **Use when**: Away from WiFi but near your phone
- **Power**: ~15-30mA during active use (10x more efficient!)
- **Note**: Weather requires WiFi - switch back to WiFi mode for weather updates

## How to Use

### On the Watch

1. Navigate to the **Settings** screen (upward swipe from watch face)
2. You'll see a **Connection** section with a toggle switch:
   - **WiFi** (left) - Default mode, connects to your WiFi network
   - **BLE** (right) - BLE mode, connects to your phone via Bluetooth
3. Toggle the switch to change modes
4. The "Mode" label will update to show the current mode

### On Your Phone

You'll need a companion app or BLE terminal to send data to the watch. Here are the BLE service UUIDs:

#### Time Sync Service (Currently Implemented)
- **Service UUID**: `12340000-1234-1234-1234-123456789abc`
- **Time Sync Characteristic**: `12340001-1234-1234-1234-123456789abc`
  - **Type**: Write
  - **Format**: 4-byte Unix timestamp (little-endian)
  - **Example**: Send `Math.floor(Date.now() / 1000)` from JavaScript

#### Future Services
- **Notification Service**: Coming when vibration motor is added
- **Music Control**: Coming in future update
- **Find My Phone**: Coming in future update

**Note**: Weather service was removed - weather requires internet, so use WiFi mode instead!

## Implementation Details

### File Structure

```
include/
  └── ble_handler.h         # BLE handler header
src/
  └── ble_handler.cpp       # BLE implementation
  └── app_manager.cpp       # Updated with BLE mode toggle
  └── main.cpp              # BLE initialization and updates
  └── wifi_handler.cpp      # Mode-aware WiFi handling
```

### Key Components

#### 1. BLE Handler (`ble_handler.cpp`)
- Manages BLE server, services, and characteristics
- Handles connection/disconnection callbacks
- Processes incoming time and weather data
- Provides mode switching functionality

#### 2. Settings Screen (`app_manager.cpp`)
- New WiFi/BLE toggle switch
- Visual mode indicator
- Event handler for mode switching

#### 3. Main Loop (`main.cpp`)
- BLE initialization
- BLE update calls
- Mode-aware time synchronization
- Mode-aware connectivity indicator

#### 4. Weather App (`app_manager.cpp`)
- Checks connection mode
- Fetches weather from WiFi API or BLE
- Displays appropriate status messages

### Mode Switching Behavior

When switching **FROM WiFi TO BLE**:
1. WiFi is disconnected and disabled
2. BLE services are started
3. Watch begins advertising as "OpenWatch"
4. WiFi icon shows BLE connection status

When switching **FROM BLE TO WiFi**:
5. BLE services are stopped and resources freed
6. WiFi reconnects automatically (handled by existing WiFi handler)
7. WiFi icon shows WiFi connection status

### Data Flow

#### Time Sync
```
Phone App → BLE Time Characteristic → ble_handler.cpp → System Time → Watch Face
```

#### Weather Data
```
Phone App → BLE Weather Characteristics → ble_handler.cpp → weather_data → Weather Screen → Watch Face
```

## No Companion App Needed!

You can sync time from your phone using existing BLE tools:

### Option 1: Use BLE Scanner Apps (Easiest)
**Android**: 
- Download "nRF Connect" from Play Store
- Connect to "OpenWatch"
- Find Time Sync characteristic (`12340001-...`)
- Write 4-byte timestamp

**iOS**:
- Download "LightBlue" from App Store  
- Connect to "OpenWatch"
- Find Time Sync characteristic
- Write timestamp as hex value

### Option 2: Web Bluetooth (Any Phone Browser)
Open this code in Chrome/Edge on your phone:

```html
<!DOCTYPE html>
<html>
<body>
<button onclick="syncTime()">Sync Time to OpenWatch</button>
<script>
let device, timeChar;

async function syncTime() {
  if (!timeChar) {
    // Connect first time
    device = await navigator.bluetooth.requestDevice({
      filters: [{ name: 'OpenWatch' }],
      optionalServices: ['12340000-1234-1234-1234-123456789abc']
    });
    const server = await device.gatt.connect();
    const service = await server.getPrimaryService('12340000-1234-1234-1234-123456789abc');
    timeChar = await service.getCharacteristic('12340001-1234-1234-1234-123456789abc');
  }
  
  // Send current time
  const timestamp = Math.floor(Date.now() / 1000);
  const buffer = new ArrayBuffer(4);
  new DataView(buffer).setUint32(0, timestamp, true);
  await timeChar.writeValue(buffer);
  alert('Time synced!');
}
</script>
</body>
</html>
```

Save this as `openwatch_sync.html` and open in your phone browser!

## Testing

### BLE Mode Testing
1. Upload firmware to watch
2. Navigate to Settings and switch to BLE mode
3. Watch should start advertising as "OpenWatch"
4. Connect from phone using BLE scanner app (e.g., nRF Connect, LightBlue)
5. Send time sync data to verify time updates
6. Send weather data to verify weather screen updates

### WiFi Mode Testing
1. Switch back to WiFi mode in Settings
2. Watch should reconnect to WiFi automatically
3. Verify time sync via NTP
4. Verify weather fetch from NOAA API

## Troubleshooting

### Watch Not Advertising
- Ensure you're in BLE mode (check Settings screen)
- Reboot the watch
- Check serial monitor for "[BLE] Services started and advertising!"

### Phone Can't Connect
- Make sure Bluetooth is enabled on phone
- Try disconnecting and reconnecting
- Check serial monitor for "[BLE] Client connected!" message

### Time Not Syncing
- Verify timestamp format (4-byte little-endian Unix time)
- Check serial monitor for "[BLE] Received time sync" message
- Try sending timestamp again (BLE sometimes drops first packet)

### Weather Not Showing
- Weather requires WiFi mode! Switch to WiFi mode in Settings
- BLE mode is for time sync and future notifications only

## Power Consumption

BLE mode is generally more power-efficient than WiFi for smartwatch applications:
- **WiFi**: ~100-300mA during active connection
- **BLE**: ~15-30mA during active connection
- **BLE Advertising**: ~10-20mA

Consider using BLE mode for better battery life when you're near your phone.

## Security Considerations

The current implementation uses **unencrypted** BLE connections. For production use, consider:
- Adding BLE pairing/bonding
- Implementing encryption for sensitive data
- Adding authentication mechanisms
- Restricting which devices can connect

## Roadmap

### Next (When Vibration Motor Added)
- [ ] Notification service - get phone notifications on watch
- [ ] Call alerts - vibrate for incoming calls
- [ ] Text message previews

### Future
- [ ] Music control - play/pause/skip from watch
- [ ] Find-my-phone - make phone ring from watch
- [ ] Battery level reporting to phone
- [ ] Secure pairing with encryption

### Won't Implement
- ~~Weather via BLE~~ - Weather needs internet, use WiFi mode
- ~~Dual WiFi+BLE~~ - Time-division multiplexing adds complexity/bugs

---

**Note**: This is a development implementation. For production use, additional security and error handling should be implemented.

