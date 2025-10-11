#pragma once
#include "display_adapter.h"

// Forward declaration
class AppManager;

// Draw the Fitness app screen. For now it simply displays the word "Fitness".
void drawFitness(Gc9Display &display);
void resetFitnessDisplay(Gc9Display &display);

// Register this app with the app manager
void registerFitnessApp(AppManager& appManager);
