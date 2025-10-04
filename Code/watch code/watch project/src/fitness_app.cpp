#include "fitness_app.h"

void drawFitness(Adafruit_SSD1306 &display) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  // Choose text size so that word fits nicely; center it.
  // We'll attempt size 2; width each character roughly 12px including spacing when size=2.
  display.setTextSize(2);
  const char *label = "Fitness";
  // Basic centering:
  int16_t x1, y1; uint16_t w, h;
  display.getTextBounds(label, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (display.width() - w) / 2;
  int16_t y = (display.height() - h) / 2;
  display.setCursor(x, y);
  display.print(label);
  display.display();
}
