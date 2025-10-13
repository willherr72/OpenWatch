#pragma once
#include "display_adapter.h"

class AppManager;

void registerCalibrationApp(AppManager &appManager);
void resetCalibrationApp(Gc9Display &display);
void drawCalibrationApp(Gc9Display &display);
void calibrationAppUpdate();
