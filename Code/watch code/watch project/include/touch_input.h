#pragma once
#include <Arduino.h>

// Gesture codes (matching CST816T definitions)
enum class TouchGesture : uint8_t {
  NONE = 0x00,
  SWIPE_UP = 0x01,
  SWIPE_DOWN = 0x02,
  SWIPE_LEFT = 0x03,
  SWIPE_RIGHT = 0x04,
  SINGLE_CLICK = 0x05,
  DOUBLE_CLICK = 0x0B,
  LONG_PRESS = 0x0C
};

struct TouchPoint {
  uint16_t x;
  uint16_t y;
  uint16_t rawX;
  uint16_t rawY;
  TouchGesture gesture;
  bool touching;
};

void touchInit();
bool touchRead(TouchPoint &point);
void touchResetState();
void touchSetRawCalibration(uint16_t minX, uint16_t maxX, uint16_t minY, uint16_t maxY,
                            bool invertX, bool invertY, bool swapXY);
void touchGetRawCalibration(uint16_t &minX, uint16_t &maxX, uint16_t &minY, uint16_t &maxY,
                            bool &invertX, bool &invertY, bool &swapXY);
