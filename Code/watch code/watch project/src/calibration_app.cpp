#include "calibration_app.h"
#include "app_manager.h"
#include "touch_input.h"
#include <math.h>

namespace {
struct CalibrationPointSpec {
  const char *label;
  int16_t x;
  int16_t y;
};

constexpr CalibrationPointSpec kTargets[] = {
  {"Top Left", 40, 40},
  {"Top Right", 200, 40},
  {"Bottom Right", 200, 200},
  {"Bottom Left", 40, 200},
  {"Center", 120, 120}
};
constexpr uint8_t kTargetCount = sizeof(kTargets) / sizeof(kTargets[0]);

enum class Phase : uint8_t {
  Collecting,
  Finished
};

struct Sample {
  uint16_t rawX = 0;
  uint16_t rawY = 0;
  bool recorded = false;
};

struct CalibrationState {
  Phase phase = Phase::Collecting;
  uint8_t currentIndex = 0;
  Sample samples[kTargetCount];
  bool needsRedraw = true;
  bool resultsApplied = false;
  uint16_t minX = 0;
  uint16_t maxX = 0;
  uint16_t minY = 0;
  uint16_t maxY = 0;
  bool invertX = false;
  bool invertY = false;
  bool swapXY = false;
  int16_t offsetX = 0;
  int16_t offsetY = 0;
  float scaleX = 1.0f;
  float scaleY = 1.0f;
  unsigned long lastTouchMillis = 0;
};

CalibrationState gState;
AppManager *gManager = nullptr;
int gAppIndex = -1;

void initializeState() {
  gState.phase = Phase::Collecting;
  gState.currentIndex = 0;
  gState.needsRedraw = true;
  gState.resultsApplied = false;
  gState.minX = gState.maxX = 0;
  gState.minY = gState.maxY = 0;
  gState.invertX = false;
  gState.invertY = false;
  gState.swapXY = false;
  gState.offsetX = 0;
  gState.offsetY = 0;
  gState.scaleX = 1.0f;
  gState.scaleY = 1.0f;
  gState.lastTouchMillis = 0;
  for (auto &sample : gState.samples) {
    sample = Sample{};
  }
  touchResetState();
}

float mapAxisFloat(uint16_t raw, uint16_t minVal, uint16_t maxVal, bool invertAxis) {
  if (maxVal <= minVal) {
    return 0.0f;
  }
  float clamped = raw;
  if (raw < minVal) clamped = static_cast<float>(minVal);
  if (raw > maxVal) clamped = static_cast<float>(maxVal);
  float normalized = (clamped - static_cast<float>(minVal)) / (static_cast<float>(maxVal - minVal));
  if (normalized < 0.0f) normalized = 0.0f;
  if (normalized > 1.0f) normalized = 1.0f;
  float mapped = normalized * 239.0f;
  if (invertAxis) {
    mapped = 239.0f - mapped;
  }
  return mapped;
}

float mapSampleX(const Sample &sample) {
  uint16_t axisRaw = gState.swapXY ? sample.rawY : sample.rawX;
  return mapAxisFloat(axisRaw, gState.minX, gState.maxX, gState.invertX);
}

float mapSampleY(const Sample &sample) {
  uint16_t axisRaw = gState.swapXY ? sample.rawX : sample.rawY;
  return mapAxisFloat(axisRaw, gState.minY, gState.maxY, gState.invertY);
}

int16_t roundToInt(float value) {
  if (value >= 0.0f) {
    return static_cast<int16_t>(value + 0.5f);
  }
  return static_cast<int16_t>(value - 0.5f);
}

void computeCalibration() {
  if (gState.currentIndex < kTargetCount) {
    return;
  }

  const Sample &topLeft = gState.samples[0];
  const Sample &topRight = gState.samples[1];
  const Sample &bottomRight = gState.samples[2];
  const Sample &bottomLeft = gState.samples[3];
  const Sample &center = gState.samples[4];

  uint32_t diffXRawX = static_cast<uint32_t>(abs(static_cast<int32_t>(topRight.rawX) - static_cast<int32_t>(topLeft.rawX))) +
                       static_cast<uint32_t>(abs(static_cast<int32_t>(bottomRight.rawX) - static_cast<int32_t>(bottomLeft.rawX)));
  uint32_t diffXRawY = static_cast<uint32_t>(abs(static_cast<int32_t>(topRight.rawY) - static_cast<int32_t>(topLeft.rawY))) +
                       static_cast<uint32_t>(abs(static_cast<int32_t>(bottomRight.rawY) - static_cast<int32_t>(bottomLeft.rawY)));
  gState.swapXY = diffXRawY > diffXRawX;

  auto axisXValue = [&](const Sample &s) -> uint16_t {
    return gState.swapXY ? s.rawY : s.rawX;
  };
  auto axisYValue = [&](const Sample &s) -> uint16_t {
    return gState.swapXY ? s.rawX : s.rawY;
  };

  uint16_t axisXVals[kTargetCount];
  uint16_t axisYVals[kTargetCount];
  for (uint8_t i = 0; i < kTargetCount; ++i) {
    axisXVals[i] = axisXValue(gState.samples[i]);
    axisYVals[i] = axisYValue(gState.samples[i]);
  }

  auto findMin = [](const uint16_t *values, uint8_t count) -> uint16_t {
    uint16_t current = values[0];
    for (uint8_t i = 1; i < count; ++i) {
      if (values[i] < current) {
        current = values[i];
      }
    }
    return current;
  };

  auto findMax = [](const uint16_t *values, uint8_t count) -> uint16_t {
    uint16_t current = values[0];
    for (uint8_t i = 1; i < count; ++i) {
      if (values[i] > current) {
        current = values[i];
      }
    }
    return current;
  };

  gState.minX = findMin(axisXVals, kTargetCount);
  gState.maxX = findMax(axisXVals, kTargetCount);
  gState.minY = findMin(axisYVals, kTargetCount);
  gState.maxY = findMax(axisYVals, kTargetCount);

  float leftRaw = (static_cast<float>(axisXVals[0]) + static_cast<float>(axisXVals[3])) * 0.5f;
  float rightRaw = (static_cast<float>(axisXVals[1]) + static_cast<float>(axisXVals[2])) * 0.5f;
  gState.invertX = leftRaw > rightRaw;

  float topRaw = (static_cast<float>(axisYVals[0]) + static_cast<float>(axisYVals[1])) * 0.5f;
  float bottomRaw = (static_cast<float>(axisYVals[3]) + static_cast<float>(axisYVals[2])) * 0.5f;
  gState.invertY = topRaw > bottomRaw;

  touchSetRawCalibration(gState.minX, gState.maxX, gState.minY, gState.maxY,
                         gState.invertX, gState.invertY, gState.swapXY);

  float mappedLeft = (mapSampleX(topLeft) + mapSampleX(bottomLeft)) * 0.5f;
  float mappedRight = (mapSampleX(topRight) + mapSampleX(bottomRight)) * 0.5f;
  float mappedTop = (mapSampleY(topLeft) + mapSampleY(topRight)) * 0.5f;
  float mappedBottom = (mapSampleY(bottomLeft) + mapSampleY(bottomRight)) * 0.5f;

  float targetLeft = (static_cast<float>(kTargets[0].x) + static_cast<float>(kTargets[3].x)) * 0.5f;
  float targetRight = (static_cast<float>(kTargets[1].x) + static_cast<float>(kTargets[2].x)) * 0.5f;
  float targetTop = (static_cast<float>(kTargets[0].y) + static_cast<float>(kTargets[1].y)) * 0.5f;
  float targetBottom = (static_cast<float>(kTargets[3].y) + static_cast<float>(kTargets[2].y)) * 0.5f;

  float spanMappedX = mappedRight - mappedLeft;
  float spanMappedY = mappedBottom - mappedTop;
  float spanTargetX = targetRight - targetLeft;
  float spanTargetY = targetBottom - targetTop;

  if (fabsf(spanMappedX) > 0.001f) {
    gState.scaleX = spanTargetX / spanMappedX;
    gState.offsetX = roundToInt(targetLeft / gState.scaleX - mappedLeft);
  } else {
    gState.scaleX = 1.0f;
    gState.offsetX = 0;
  }

  if (fabsf(spanMappedY) > 0.001f) {
    gState.scaleY = spanTargetY / spanMappedY;
    gState.offsetY = roundToInt(targetTop / gState.scaleY - mappedTop);
  } else {
    gState.scaleY = 1.0f;
    gState.offsetY = 0;
  }

  if (gManager) {
    gManager->setTouchCalibration(gState.offsetX, gState.offsetY, gState.scaleX, gState.scaleY);
  }

  float centerMappedX = mapSampleX(center);
  float centerMappedY = mapSampleY(center);
  float centerAdjustedX = (centerMappedX + static_cast<float>(gState.offsetX)) * gState.scaleX;
  float centerAdjustedY = (centerMappedY + static_cast<float>(gState.offsetY)) * gState.scaleY;

  Serial.println(F("--- Touch Calibration Complete ---"));
  Serial.printf("TOUCH_RAW_MIN_X=%u\n", gState.minX);
  Serial.printf("TOUCH_RAW_MAX_X=%u\n", gState.maxX);
  Serial.printf("TOUCH_RAW_MIN_Y=%u\n", gState.minY);
  Serial.printf("TOUCH_RAW_MAX_Y=%u\n", gState.maxY);
  Serial.printf("TOUCH_INVERT_X=%d\n", gState.invertX ? 1 : 0);
  Serial.printf("TOUCH_INVERT_Y=%d\n", gState.invertY ? 1 : 0);
  Serial.printf("TOUCH_SWAP_XY=%d\n", gState.swapXY ? 1 : 0);
  Serial.printf("TOUCH_CALIB_X_OFFSET=%d\n", gState.offsetX);
  Serial.printf("TOUCH_CALIB_Y_OFFSET=%d\n", gState.offsetY);
  Serial.printf("TOUCH_CALIB_X_SCALE=%.3f\n", gState.scaleX);
  Serial.printf("TOUCH_CALIB_Y_SCALE=%.3f\n", gState.scaleY);
  Serial.printf("Center error: dX=%.2f, dY=%.2f\n",
                static_cast<double>(centerAdjustedX - static_cast<float>(kTargets[4].x)),
                static_cast<double>(centerAdjustedY - static_cast<float>(kTargets[4].y)));
  Serial.println(F("Add the TOUCH_* defines above to platformio.ini build_flags to persist."));

  gState.resultsApplied = true;
  gState.phase = Phase::Finished;
  gState.needsRedraw = true;
  touchResetState();
}

void drawCrosshair(Gc9Display &display, int16_t cx, int16_t cy) {
  const int16_t radius = 16;
  display.drawCircle(cx, cy, radius, COLOR_WHITE);
  display.drawFastHLine(cx - radius, cy, radius * 2, COLOR_WHITE);
  display.drawFastVLine(cx, cy - radius, radius * 2, COLOR_WHITE);
  display.fillCircle(cx, cy, 3, COLOR_WHITE);
}

} // namespace

void resetCalibrationApp(Gc9Display &display) {
  initializeState();
  drawCalibrationApp(display);
}

void drawCalibrationApp(Gc9Display &display) {
  if (!gState.needsRedraw) {
    return;
  }

  display.fillScreen(COLOR_BLACK);
  display.setTextColor(COLOR_WHITE);
  display.setTextSize(2);
  display.setCursor(30, 16);
  display.print(F("Touch Cal"));

  display.setTextSize(1);

  if (gState.phase == Phase::Collecting) {
    uint8_t step = gState.currentIndex + 1;
    if (step > kTargetCount) step = kTargetCount;
    char line[32];
    snprintf(line, sizeof(line), "Tap target %u/%u", static_cast<unsigned>(step), static_cast<unsigned>(kTargetCount));
    display.setCursor(20, 48);
    display.print(line);
    display.setCursor(20, 64);
    display.print(F("Use clean taps."));

    if (gState.currentIndex < kTargetCount) {
      display.setCursor(20, 80);
      display.print(kTargets[gState.currentIndex].label);
      drawCrosshair(display, kTargets[gState.currentIndex].x, kTargets[gState.currentIndex].y);
    }

    display.setCursor(20, display.height() - 24);
    display.print(F("Menu button: exit"));
  } else {
    char line[48];
    display.setCursor(20, 48);
    display.print(F("Calibration done."));

    snprintf(line, sizeof(line), "RawX:%u-%u", gState.minX, gState.maxX);
    display.setCursor(20, 72);
    display.print(line);

    snprintf(line, sizeof(line), "RawY:%u-%u", gState.minY, gState.maxY);
    display.setCursor(20, 88);
    display.print(line);

    snprintf(line, sizeof(line), "Swap=%d InvX=%d InvY=%d", gState.swapXY ? 1 : 0,
             gState.invertX ? 1 : 0, gState.invertY ? 1 : 0);
    display.setCursor(20, 104);
    display.print(line);

    snprintf(line, sizeof(line), "OffX=%d OffY=%d", gState.offsetX, gState.offsetY);
    display.setCursor(20, 120);
    display.print(line);

    snprintf(line, sizeof(line), "ScaleX=%.2f", static_cast<double>(gState.scaleX));
    display.setCursor(20, 136);
    display.print(line);

    snprintf(line, sizeof(line), "ScaleY=%.2f", static_cast<double>(gState.scaleY));
    display.setCursor(20, 152);
    display.print(line);

    display.setCursor(20, 176);
    display.print(F("Tap screen to rerun."));
    display.setCursor(20, 192);
    display.print(F("Menu button: exit"));
  }

  gState.needsRedraw = false;
  display.display();
}

void calibrationAppUpdate() {
  if (!gManager || gAppIndex < 0) {
    return;
  }
  if (gManager->isMenuActive()) {
    return;
  }
  if (gManager->currentAppIndex() != gAppIndex) {
    return;
  }

  TouchPoint point{};
  if (!touchRead(point)) {
    return;
  }

  gState.lastTouchMillis = millis();

  if (gState.phase == Phase::Collecting) {
    if (gState.currentIndex < kTargetCount) {
      gState.samples[gState.currentIndex].rawX = point.rawX;
      gState.samples[gState.currentIndex].rawY = point.rawY;
      gState.samples[gState.currentIndex].recorded = true;
      gState.currentIndex++;
      gState.needsRedraw = true;
      if (gState.currentIndex >= kTargetCount) {
        computeCalibration();
      }
    }
  } else if (gState.phase == Phase::Finished) {
    initializeState();
    gState.needsRedraw = true;
  }
}

void registerCalibrationApp(AppManager &appManager) {
  gManager = &appManager;
  initializeState();
  int index = appManager.getAppCount();
  App calibrationApp = {
    "Calibrate",         // name
    drawCalibrationApp,   // drawFunction
    calibrationAppUpdate, // updateFunction
    nullptr,              // buttonHandler
    false,                // isSpecial
    resetCalibrationApp   // resetFunction
  };

  if (appManager.registerApp(calibrationApp)) {
    gAppIndex = index;
  } else {
    gAppIndex = -1;
  }
}
