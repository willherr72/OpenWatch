#pragma once
#include <Arduino.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>

// Common colors
#ifndef COLOR_BLACK
#define COLOR_BLACK 0x0000
#endif
#ifndef COLOR_WHITE
#define COLOR_WHITE 0xFFFF
#endif
#ifndef COLOR_BLUE
#define COLOR_BLUE 0x001F
#endif
#ifndef COLOR_GREEN
#define COLOR_GREEN 0x07E0
#endif
#ifndef COLOR_RED
#define COLOR_RED 0xF800
#endif
#ifndef COLOR_LIGHTGRAY
#define COLOR_LIGHTGRAY 0xC618
#endif
#ifndef COLOR_DARKGRAY
#define COLOR_DARKGRAY 0x7BEF
#endif

// Adapter class to mimic a subset of SSD1306 API used in this project
class Gc9Display : public Adafruit_GC9A01A {
public:
  using Adafruit_GC9A01A::Adafruit_GC9A01A;

  // SSD1306 compatibility helpers
  inline void clearDisplay() { fillScreen(COLOR_BLACK); }
  inline void display() {} // no framebuffer push needed
  inline void ssd1306_command(uint8_t) { /* no-op for TFT */ }
};
