#pragma once
#include <Arduino.h>

// ============================================================
// TOUCH GESTURE SYSTEM - CST816T Touch Controller
// ============================================================
// 
// This file defines all gesture types supported by the CST816T
// touch controller. The controller runs in MIXED mode, which
// provides both gesture detection AND touch coordinate tracking.
//
// All gesture codes match the CST816T hardware specification.
// See GESTURE_GUIDE.md for detailed gesture documentation.
//
// ============================================================

// Gesture codes (matching CST816T hardware specification)
// Reference: CST816T datasheet and Waveshare examples
enum class TouchGesture : uint8_t {
  NONE = 0x00,           // No gesture detected
  SWIPE_UP = 0x01,       // Upward swipe motion
  SWIPE_DOWN = 0x02,     // Downward swipe motion
  SWIPE_LEFT = 0x03,     // Leftward swipe motion
  SWIPE_RIGHT = 0x04,    // Rightward swipe motion
  SINGLE_CLICK = 0x05,   // Single tap/click on screen
  DOUBLE_CLICK = 0x0B,   // Double tap/double click (0x0B from hardware or software detection)
  LONG_PRESS = 0x0C      // Long press/hold gesture
};

struct TouchPoint {
  uint16_t x;
  uint16_t y;
  uint16_t rawX;
  uint16_t rawY;
  TouchGesture gesture;
  bool touching;
};

// Helper functions for gesture names (for debugging/logging)
inline const char* gestureToString(TouchGesture gesture) {
  switch (gesture) {
    case TouchGesture::NONE:         return "NONE";
    case TouchGesture::SWIPE_UP:     return "SWIPE_UP";
    case TouchGesture::SWIPE_DOWN:   return "SWIPE_DOWN";
    case TouchGesture::SWIPE_LEFT:   return "SWIPE_LEFT";
    case TouchGesture::SWIPE_RIGHT:  return "SWIPE_RIGHT";
    case TouchGesture::SINGLE_CLICK: return "SINGLE_CLICK";
    case TouchGesture::DOUBLE_CLICK: return "DOUBLE_CLICK";
    case TouchGesture::LONG_PRESS:   return "LONG_PRESS";
    default:                         return "UNKNOWN";
  }
}

void touchInit();
bool touchRead(TouchPoint &point);
void touchResetState();
void touchSetRawCalibration(uint16_t minX, uint16_t maxX, uint16_t minY, uint16_t maxY,
                            bool invertX, bool invertY, bool swapXY);
void touchGetRawCalibration(uint16_t &minX, uint16_t &maxX, uint16_t &minY, uint16_t &maxY,
                            bool &invertX, bool &invertY, bool &swapXY);
