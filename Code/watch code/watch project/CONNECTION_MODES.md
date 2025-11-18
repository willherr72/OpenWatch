# OpenWatch Connection Modes - Quick Reference

## TL;DR
- **WiFi Mode** (default): Time + Weather ✅
- **BLE Mode** (battery saver): Time only, notifications later 🚧

## When to Use Each Mode

### Use WiFi Mode When:
- 🏠 At home or office with WiFi
- 🌤️ You want weather updates
- 🌐 You need full internet features
- 🔌 Near a charger (uses more power)

### Use BLE Mode When:
- 🚶 Away from WiFi but near your phone
- 🔋 Want to save battery (10x more efficient!)
- ⏰ Only need time sync
- 📱 Waiting for notification features (coming soon)

## How to Switch Modes

1. Swipe **UP** from watch face → Settings
2. Toggle switch: **WiFi** ← → **BLE**
3. Current mode shown below switch

## What Works in Each Mode

| Feature | WiFi Mode | BLE Mode |
|---------|-----------|----------|
| Time Sync | ✅ NTP (auto) | ✅ From phone (manual) |
| Weather | ✅ NOAA API | ❌ Need WiFi |
| Battery Life | ~8-12 hours | ~80-120 hours |
| Notifications | ❌ | 🚧 Coming soon |

## Time Sync in BLE Mode

Since you don't have WiFi in BLE mode, sync time from your phone:

### Quick Method (No App Needed!)
1. Open Chrome on your phone
2. Go to: `https://googlechromelabs.github.io/web-bluetooth/demos/`
3. Find "Generic Access" demo
4. Connect to "OpenWatch"
5. Write timestamp to characteristic `12340001-1234-1234-1234-123456789abc`

### Using BLE Scanner App
1. Download "nRF Connect" (Android) or "LightBlue" (iOS)
2. Connect to "OpenWatch"
3. Find Time Service → Time Sync characteristic
4. Write current Unix timestamp (4 bytes, little-endian)

**Tip**: Your phone likely has apps that can do this automatically!

## Technical Details

### Why Not Both at Once?
ESP32-S3 CAN run WiFi+BLE simultaneously, but it uses time-division multiplexing which causes:
- Packet delays and dropped connections
- Timing bugs and race conditions  
- Unpredictable performance

Clean separation = reliable operation! (Same approach as Apple Watch, Samsung Galaxy Watch)

### Power Consumption
- **WiFi active**: 100-300mA
- **BLE active**: 15-30mA
- **Deep sleep**: <1mA (future)

Switching to BLE mode when away from WiFi can **extend battery life by 10x**!

## Future Features (BLE Mode)

When vibration motor is added:
- 📱 Phone call alerts
- 💬 Text message notifications
- 📧 Email previews
- 🎵 Music control
- 📍 Find my phone

## Common Issues

**Q: Weather shows "Weather needs WiFi" in BLE mode**  
A: Correct! Weather requires internet. Switch to WiFi mode for weather.

**Q: Time not syncing in BLE mode**  
A: You need to manually sync from phone. See "Time Sync in BLE Mode" above.

**Q: Can I leave it in WiFi mode all the time?**  
A: Yes! WiFi is the default and works great when you have WiFi available.

**Q: Does BLE mode use my phone's data?**  
A: No! BLE is direct watch↔phone connection. No cellular data used.

## Recommendation

**Start with WiFi mode** (default) - it just works!  
Switch to **BLE mode** when you're away from WiFi to save battery.

---

*For detailed technical documentation, see `BLE_IMPLEMENTATION.md`*

