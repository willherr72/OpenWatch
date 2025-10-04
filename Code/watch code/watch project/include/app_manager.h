#pragma once
#include <Arduino.h>
#include <Adafruit_SSD1306.h>
#include <vector>
#include <functional>

enum class AppId : uint8_t {
  CLOCK = 0,
  FITNESS,    // fitness app
  TIMER,      // countdown timer app
  STOCKS,     // stock prices app
  WEATHER,    // weather app
  COUNT
};

class AppManager {
public:
  AppManager();
  void next();
  void previous();
  void select();
  void exitMenu();
  void enterMenu();
  bool isMenuActive() const { return inMenu; }
  AppId currentApp() const { return activeApp; }
  void draw(Adafruit_SSD1306 &display);
  void drawActiveApp(Adafruit_SSD1306 &display, bool &timeSynced, void (*drawClock)(Adafruit_SSD1306&, bool&));
  // Force return to clock app and exit any active menu
  void resetToClock();
private:
  bool inMenu;
  AppId activeApp;   // currently running app
  int menuIndex;     // index while navigating menu
  const char* getAppName(AppId id) const;
};
