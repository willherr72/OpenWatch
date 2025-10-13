#include "fitness_app.h"
#include "app_manager.h"

namespace {
bool fitnessNeedsRedraw = true;
}

void resetFitnessDisplay(Gc9Display &display) {
  fitnessNeedsRedraw = true;
  display.fillScreen(COLOR_BLACK);
}

void drawFitness(Gc9Display &display) {
  if (!fitnessNeedsRedraw) {
    return;
  }

  display.fillScreen(COLOR_BLACK);
  display.setTextColor(COLOR_WHITE);
  display.setTextSize(2);
  const char *label = "Fitness";
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (display.width() - static_cast<int16_t>(w)) / 2;
  int16_t y = (display.height() - static_cast<int16_t>(h)) / 2;
  display.setCursor(x, y);
  display.print(label);
  display.display();

  fitnessNeedsRedraw = false;
}

void registerFitnessApp(AppManager& appManager) {
  App fitnessApp = {
    "Fitness",      // name
    drawFitness,    // drawFunction
    nullptr,        // updateFunction (not needed)
    nullptr,        // buttonHandler (not needed)
    false,          // isSpecial (not the clock app)
    resetFitnessDisplay // resetFunction
  };
  
  appManager.registerApp(fitnessApp);
}
