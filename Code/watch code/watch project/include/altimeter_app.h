#pragma once
#include "display_adapter.h"

// Forward declaration
class AppManager;

// Draw the Altimeter app screen with BMP280 sensor readings
void drawAltimeter(Gc9Display &display);
void resetAltimeterDisplay(Gc9Display &display);
void updateAltimeter();

// Register this app with the app manager
void registerAltimeterApp(AppManager& appManager);
