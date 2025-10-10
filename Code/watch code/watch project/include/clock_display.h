#pragma once
#include <Arduino.h>
#include "display_adapter.h"

bool fetchLocalTime(struct tm &out, uint32_t timeoutMs = 1500);
void drawCurrentTime(Gc9Display &display, bool &timeSynced);
void resetClockDisplay(Gc9Display &display);
void showMessage(Gc9Display &display, const __FlashStringHelper* msg);