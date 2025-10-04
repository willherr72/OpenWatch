#pragma once
#include <Arduino.h>
#include <Adafruit_SSD1306.h>

bool fetchLocalTime(struct tm &out, uint32_t timeoutMs = 1500);
void drawCurrentTime(Adafruit_SSD1306 &display, bool &timeSynced);
void showMessage(Adafruit_SSD1306 &display, const __FlashStringHelper* msg);