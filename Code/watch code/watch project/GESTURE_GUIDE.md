# OpenWatch Touch Gesture Guide

## Implemented Gestures

All gestures are defined in `include/touch_input.h` as part of the `TouchGesture` enum.

### Gesture Types and Codes

| Gesture | Code | Hex | Description | Status |
|---------|------|-----|-------------|--------|
| NONE | 0x00 | 0x00 | No gesture detected | ✓ |
| SWIPE_UP | 0x01 | 0x01 | Upward swipe motion | ✓ Implementation Ready |
| SWIPE_DOWN | 0x02 | 0x02 | Downward swipe motion | ✓ Implementation Ready |
| SWIPE_LEFT | 0x03 | 0x03 | Leftward swipe motion | ✓ Implementation Ready |
| SWIPE_RIGHT | 0x04 | 0x04 | Rightward swipe motion | ✓ Used (Open Menu) |
| SINGLE_CLICK | 0x05 | 0x05 | Single tap on screen | ✓ Implementation Ready |
| DOUBLE_CLICK | 0x0B | 0x0B | Double tap on screen | ✓ Used (Tap & Hold) |
| LONG_PRESS | 0x0C | 0x0C | Long press hold | ✓ Implementation Ready |

## Currently Used Gestures

### Menu
- **SWIPE_UP** → Previous app
- **SWIPE_DOWN** → Next app
- **SWIPE_LEFT** → Enter selected app
- **SWIPE_RIGHT** → Enter selected app

### Timer App
- **SWIPE_UP** → Previous preset (1m → 5m → 10m)
- **SWIPE_DOWN** → Next preset
- **Tap & Hold (500ms)** → Start timer (in SELECTING state)
- **SWIPE_DOWN** → Cancel timer (in RUNNING state)
- **Tap & Hold (500ms)** → Reset timer (in DONE state)

### Clock App
- Available for custom implementation

## Available for Implementation

The following gestures are detected and logged but not yet assigned to functions:

- **SINGLE_CLICK** (0x05)
- **LONG_PRESS** (0x0C)
- **SWIPE_LEFT** (in most contexts, currently available only in menu for app entry)

## Gesture Detection Pipeline

1. **Hardware Detection** (CST816T in MIXED mode)
   - Detects swipes, clicks, and long-press
   - Returns gesture code and touch coordinates

2. **Software Enhancement**
   - Double-tap detection with 350ms window
   - Tap-and-hold tracking (500ms for timer)
   - Touch coordinate smoothing (4-sample average)

3. **Gesture Deduplication**
   - Prevents repeated processing of same gesture
   - Clears gesture state after 50ms of no input

4. **Logging**
   - All gestures logged to serial with readable names
   - Format: `[GESTURE] GESTURE_NAME at (x, y)`
   - Hardware gestures also logged with codes

## Adding New Gesture Handlers

### In Menu (app_manager.cpp)
```cpp
case GESTURE_SINGLE_CLICK:
  // Handle single click in menu
  Serial.println("Menu: Single click");
  break;
```

### In Timer App (timer_app.cpp)
```cpp
if (touchPoint.gesture == TouchGesture::LONG_PRESS) {
  // Handle long press in timer
  Serial.println("Timer: Long press detected");
}
```

### In Clock App (clock_display.cpp)
```cpp
if (touchPoint.gesture == TouchGesture::SINGLE_CLICK) {
  // Handle single click on clock
  Serial.println("Clock: Single click detected");
}
```

## Gesture Configuration

The CST816T can be configured in three modes:

- **Mode 0**: Gesture only (no coordinates)
- **Mode 1**: Point mode (coordinates only)
- **Mode 2**: Mixed mode (both - currently used) ✓

Current configuration in `touch_input.cpp`:
```cpp
CST816_MODE_MIXED  // Enables both gestures and touch coordinates
```

## Debugging

Enable serial monitor to see all detected gestures:

1. Build and upload
2. Open Serial Monitor at 115200 baud
3. Observe gesture detection in real-time:
   ```
   GESTURE DETECTED: gesture=0x01 (SWIPE_UP), raw(120,100) smooth(119,101) mapped(120,100) touching=0
   [GESTURE] SWIPE_UP at (120, 100)
   ```

## Performance Notes

- Touch input polled every loop iteration for accurate double-tap detection
- Display updated at 500ms interval (separate from touch processing)
- Gesture deduplication prevents redundant processing
- Serial logging can be disabled for production to save CPU cycles
