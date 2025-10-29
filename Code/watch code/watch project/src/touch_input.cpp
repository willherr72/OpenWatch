#include "touch_input.h"
#include <Wire.h>
#include "CST816S.h"

#ifndef TOUCH_SDA_PIN
#define TOUCH_SDA_PIN SDA
#endif

#ifndef TOUCH_SCL_PIN
#define TOUCH_SCL_PIN SCL
#endif

#ifndef TOUCH_INT_PIN
#define TOUCH_INT_PIN -1
#endif

#ifndef TOUCH_RST_PIN
#define TOUCH_RST_PIN -1
#endif

#ifndef TOUCH_I2C_ADDRESS
#ifdef TOUCH_I2C_ADDR
#define TOUCH_I2C_ADDRESS TOUCH_I2C_ADDR
#else
#define TOUCH_I2C_ADDRESS 0x15
#endif
#endif

#ifndef TOUCH_RAW_MIN_X
#define TOUCH_RAW_MIN_X 0
#endif
#ifndef TOUCH_RAW_MAX_X
#define TOUCH_RAW_MAX_X 4095
#endif
#ifndef TOUCH_RAW_MIN_Y
#define TOUCH_RAW_MIN_Y 0
#endif
#ifndef TOUCH_RAW_MAX_Y
#define TOUCH_RAW_MAX_Y 4095
#endif
#ifndef TOUCH_INVERT_X
#define TOUCH_INVERT_X 0
#endif
#ifndef TOUCH_INVERT_Y
#define TOUCH_INVERT_Y 0
#endif
#ifndef TOUCH_SWAP_XY
#define TOUCH_SWAP_XY 0
#endif

namespace {
CST816SDriver touchDriver;
bool lastDeliveredTouch = false;
constexpr uint16_t kTouchMappedMax = 239;
uint16_t rawMinX = TOUCH_RAW_MIN_X;
uint16_t rawMaxX = TOUCH_RAW_MAX_X;
uint16_t rawMinY = TOUCH_RAW_MIN_Y;
uint16_t rawMaxY = TOUCH_RAW_MAX_Y;
bool invertAxisX = TOUCH_INVERT_X != 0;
bool invertAxisY = TOUCH_INVERT_Y != 0;
bool swapAxes = TOUCH_SWAP_XY != 0;
unsigned long inputReadyTimeMs = 0;

struct SampleHistory {
  static constexpr size_t kMaxSamples = 4;
  uint16_t xs[kMaxSamples];
  uint16_t ys[kMaxSamples];
  size_t count = 0;
  size_t index = 0;

  void reset() {
    count = 0;
    index = 0;
  }

  void add(uint16_t x, uint16_t y) {
    xs[index] = x;
    ys[index] = y;
    index = (index + 1) % kMaxSamples;
    if (count < kMaxSamples) {
      ++count;
    }
  }

  void average(uint16_t &xOut, uint16_t &yOut) const {
    if (count == 0) {
      xOut = 0;
      yOut = 0;
      return;
    }
    uint32_t sumX = 0;
    uint32_t sumY = 0;
    for (size_t i = 0; i < count; ++i) {
      sumX += xs[i];
      sumY += ys[i];
    }
    xOut = static_cast<uint16_t>(sumX / count);
    yOut = static_cast<uint16_t>(sumY / count);
  }
} sampleHistory;
const uint8_t candidateAddresses[] = {
  static_cast<uint8_t>(TOUCH_I2C_ADDRESS),
  0x15,
  0x5D,
  0x2A,
  0x38
};

// Double-tap detection state
struct DoubleTapDetector {
  unsigned long lastTouchEndTime = 0;
  uint16_t lastTouchX = 0;
  uint16_t lastTouchY = 0;
  bool lastWasTouching = false;
  bool doubleTapDetected = false;
  
  // Track first tap for accurate double-tap
  unsigned long t0 = 0;
  uint16_t sx0 = 0;
  uint16_t sy0 = 0;
  
  const uint16_t TAP_RADIUS = 15;      // pixels - max movement for tap (from reference)
  const uint16_t TAP_TIME_MS = 200;    // max tap press duration (from reference)
  const uint16_t DOUBLE_TAP_WINDOW = 350;  // max time between taps
  
  // Returns true if a double-tap is detected
  bool update(uint16_t x, uint16_t y, bool touching) {
    doubleTapDetected = false;
    unsigned long now = millis();
    
    if (touching && !lastWasTouching) {
      // Touch started - check if this could be 2nd tap
      unsigned long timeSinceFirstTap = now - lastTouchEndTime;
      
      // Check if previous tap was valid and this is within double-tap window
      if (timeSinceFirstTap < DOUBLE_TAP_WINDOW && lastTouchEndTime > 0) {
        // Calculate distance from first tap
        int16_t dx = static_cast<int16_t>(x) - static_cast<int16_t>(lastTouchX);
        int16_t dy = static_cast<int16_t>(y) - static_cast<int16_t>(lastTouchY);
        int16_t distance = abs(dx) + abs(dy);  // Manhattan distance
        
        if (distance < TAP_RADIUS) {
          // This looks like a double-tap! Record it
          doubleTapDetected = true;
          Serial.printf("DOUBLE-TAP DETECTED: first tap at (%u,%u), second tap at (%u,%u), distance=%d\n", 
                        lastTouchX, lastTouchY, x, y, distance);
          lastTouchEndTime = 0;  // Reset to avoid triple-taps
          return true;
        }
      }
      
      // Otherwise, this is the first tap - record it
      t0 = now;
      sx0 = x;
      sy0 = y;
      lastTouchX = x;
      lastTouchY = y;
      
    } else if (!touching && lastWasTouching) {
      // Touch ended - check if it was a valid tap
      unsigned long tapDuration = now - t0;
      
      if (tapDuration <= TAP_TIME_MS) {
        // This was a valid tap, record end time for double-tap window
        lastTouchEndTime = now;
        Serial.printf("TAP REGISTERED: duration=%lu ms at (%u,%u)\n", tapDuration, sx0, sy0);
      } else {
        // Tap was too long, probably a drag - reset double-tap state
        lastTouchEndTime = 0;
        Serial.printf("LONG PRESS/DRAG (not a tap): duration=%lu ms\n", tapDuration);
      }
    }
    
    lastWasTouching = touching;
    return false;
  }
} doubleTapDetector;


void mapToDisplay(uint16_t rawX, uint16_t rawY, uint16_t &mappedX, uint16_t &mappedY) {
  auto clampRaw = [](uint16_t value, uint16_t minVal, uint16_t maxVal) -> uint16_t {
    if (maxVal <= minVal) {
      return minVal;
    }
    if (value < minVal) return minVal;
    if (value > maxVal) return maxVal;
    return value;
  };

  auto mapAxis = [](uint16_t value, uint16_t minVal, uint16_t maxVal) -> uint16_t {
    if (maxVal <= minVal) {
      return 0;
    }
    uint32_t span = static_cast<uint32_t>(maxVal - minVal);
    uint32_t delta = static_cast<uint32_t>(value - minVal);
    return static_cast<uint16_t>((delta * kTouchMappedMax) / span);
  };

  uint16_t rawForX = swapAxes ? rawY : rawX;
  uint16_t rawForY = swapAxes ? rawX : rawY;

  rawForX = clampRaw(rawForX, rawMinX, rawMaxX);
  rawForY = clampRaw(rawForY, rawMinY, rawMaxY);

  uint16_t baseX = mapAxis(rawForX, rawMinX, rawMaxX);
  uint16_t baseY = mapAxis(rawForY, rawMinY, rawMaxY);

  if (invertAxisX) {
    baseX = kTouchMappedMax - baseX;
  }
  if (invertAxisY) {
    baseY = kTouchMappedMax - baseY;
  }

  mappedX = baseX;
  mappedY = baseY;
}
}

void touchInit() {
  Serial.println("Initializing CST816T touch controller...");
  
  // Perform I2C scan to find devices
  Serial.println("[I2C] Scanning for devices...");
  Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);
  Wire.setClock(100000);
  int devicesFound = 0;
  for (uint8_t addr = 1; addr < 127; addr++) {
    Wire.beginTransmission(addr);
    uint8_t error = Wire.endTransmission();
    if (error == 0) {
      Serial.printf("[I2C] Device found at address 0x%02X\n", addr);
      devicesFound++;
    }
  }
  if (devicesFound == 0) {
    Serial.println("[I2C] No I2C devices found! Check wiring and pull-up resistors.");
  } else {
    Serial.printf("[I2C] Scan complete. Found %d device(s)\n", devicesFound);
  }
  
  // Use MIXED mode to enable both touch coordinates and gestures (including double-click)
  // Correct parameter order: Wire, SDA, SCL, RST, INT, mode
  if (touchDriver.begin(Wire, TOUCH_SDA_PIN, TOUCH_SCL_PIN, TOUCH_RST_PIN, TOUCH_INT_PIN, CST816_MODE_MIXED)) {
    Serial.printf("CST816T initialized at address 0x%02X in MIXED mode\n", touchDriver.activeAddress());
    inputReadyTimeMs = millis() + 80; // Warm-up delay
  } else {
    Serial.println("CST816T init FAILED");
  }
}

bool touchRead(TouchPoint &point) {
  static unsigned long lastProbeMs = 0;
  static unsigned long lastDebugMs = 0;
  constexpr unsigned long kPollIntervalMs = 30;
  unsigned long now = millis();
  
  // Initialize point
  point.x = 0;
  point.y = 0;
  point.rawX = 0;
  point.rawY = 0;
  point.gesture = TouchGesture::NONE;
  point.touching = false;
  
  // Debug logging every 5 seconds when no touch
  if (now - lastDebugMs > 5000) {
    Serial.printf("Touch: ready=%d, warmup=%d\n", touchDriver.isReady(), now >= inputReadyTimeMs);
    lastDebugMs = now;
  }
  
  if (now < inputReadyTimeMs) {
    lastDeliveredTouch = false;
    return false;
  }
  
  if (!touchDriver.isReady()) {
    Serial.println("Touch: Driver not ready!");
    lastDeliveredTouch = false;
    return false;
  }
  
  if (!touchDriver.dataReady()) {
    if (now - lastProbeMs < kPollIntervalMs) {
      lastDeliveredTouch = false;
      return false;
    }
  }
  lastProbeMs = now;
  
  // Read touch data with gesture support
  CST816SRawPoint rawPoint;
  if (!touchDriver.readTouch(rawPoint)) {
    lastDeliveredTouch = false;
    sampleHistory.reset();
    return false;
  }

  // If not touching, clear state and return false
  if (!rawPoint.touching) {
    lastDeliveredTouch = false;
    sampleHistory.reset();
    return false;
  }

  // For menu navigation, we want to allow continuous touch reading
  // Only filter out repeated touches if they're at the exact same position
  static uint16_t lastRawX = 0;
  static uint16_t lastRawY = 0;
  
  // If this is a repeated touch at the same position, skip it
  if (rawPoint.touching && lastDeliveredTouch) {
    if (rawPoint.x == lastRawX && rawPoint.y == lastRawY) {
      return false;  // Same position, don't re-deliver
    }
  }

  lastDeliveredTouch = rawPoint.touching;
  lastRawX = rawPoint.x;
  lastRawY = rawPoint.y;
  
  sampleHistory.add(rawPoint.x, rawPoint.y);
  uint16_t smoothX = rawPoint.x;
  uint16_t smoothY = rawPoint.y;
  sampleHistory.average(smoothX, smoothY);

  uint16_t mappedX = 0;
  uint16_t mappedY = 0;
  mapToDisplay(smoothX, smoothY, mappedX, mappedY);

  point.x = mappedX;
  point.y = mappedY;
  point.rawX = smoothX;
  point.rawY = smoothY;
  point.touching = rawPoint.touching;
  point.gesture = static_cast<TouchGesture>(rawPoint.gesture);
  
  // Software-based double-tap detection
  if (doubleTapDetector.update(mappedX, mappedY, rawPoint.touching)) {
    // Double-tap detected - override hardware gesture
    point.gesture = TouchGesture::DOUBLE_CLICK;
    Serial.println("SOFT DOUBLE-TAP DETECTED!");
  }
  
  // Debug: log ALL gestures with readable names
  if (rawPoint.gesture != 0x00) {
    Serial.printf("GESTURE DETECTED: gesture=0x%02X (%s), raw(%u,%u) smooth(%u,%u) mapped(%u,%u) touching=%d\n",
                  rawPoint.gesture, gestureToString(point.gesture), 
                  rawPoint.x, rawPoint.y, smoothX, smoothY, mappedX, mappedY, rawPoint.touching);
  }
  
  // Debug: log successful touch reads every 500ms
  static unsigned long lastTouchDebugMs = 0;
  if (now - lastTouchDebugMs > 500 && rawPoint.touching) {
    Serial.printf("Touch OK: raw(%u,%u) smooth(%u,%u) mapped(%u,%u) gesture=%u\n",
                  rawPoint.x, rawPoint.y, smoothX, smoothY, mappedX, mappedY, rawPoint.gesture);
    lastTouchDebugMs = now;
  }
  
  return true;
}

void touchResetState() {
  lastDeliveredTouch = false;
  sampleHistory.reset();
  doubleTapDetector.lastTouchEndTime = 0;
  doubleTapDetector.lastWasTouching = false;
  doubleTapDetector.doubleTapDetected = false;
  inputReadyTimeMs = millis() + 80;
  touchDriver.resetState();
}

void touchSetRawCalibration(uint16_t minX, uint16_t maxX, uint16_t minY, uint16_t maxY,
                            bool invertX, bool invertY, bool swapXY) {
  rawMinX = minX;
  rawMaxX = maxX;
  rawMinY = minY;
  rawMaxY = maxY;
  invertAxisX = invertX;
  invertAxisY = invertY;
  swapAxes = swapXY;
}

void touchGetRawCalibration(uint16_t &minX, uint16_t &maxX, uint16_t &minY, uint16_t &maxY,
                            bool &invertX, bool &invertY, bool &swapXY) {
  minX = rawMinX;
  maxX = rawMaxX;
  minY = rawMinY;
  maxY = rawMaxY;
  invertX = invertAxisX;
  invertY = invertAxisY;
  swapXY = swapAxes;
}
