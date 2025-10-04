#include "app_manager.h"
#include "fitness_app.h"
#include "timer_app.h"
#include "stocks_app.h"
#include "weather_app.h"

AppManager::AppManager() : inMenu(false), activeApp(AppId::CLOCK), menuIndex(0) {}

void AppManager::enterMenu() {
  inMenu = true;
  menuIndex = static_cast<int>(activeApp); // start highlight on current app
}

void AppManager::exitMenu() {
  inMenu = false;
}

void AppManager::next() {
  if (!inMenu) return;
  menuIndex = (menuIndex + 1) % static_cast<int>(AppId::COUNT);
}

void AppManager::previous() {
  if (!inMenu) return;
  menuIndex = (menuIndex - 1 + static_cast<int>(AppId::COUNT)) % static_cast<int>(AppId::COUNT);
}

void AppManager::select() {
  if (!inMenu) return;
  activeApp = static_cast<AppId>(menuIndex);
  inMenu = false; // leave menu on selection
}

const char* AppManager::getAppName(AppId id) const {
  switch(id) {
    case AppId::CLOCK: return "Clock";
    case AppId::FITNESS: return "Fitness";
    case AppId::TIMER: return "Timer";
    case AppId::STOCKS: return "Stocks";
    case AppId::WEATHER: return "Weather";
    default: return "?";
  }
}

void AppManager::draw(Adafruit_SSD1306 &display) {
  // Simple vertical list menu
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  display.println(F("Menu"));
  display.drawLine(0,10,127,10,SSD1306_WHITE);
  // For each app
  for (int i = 0; i < static_cast<int>(AppId::COUNT); ++i) {
    int y = 12 + i*10;
    if (i == menuIndex) {
      // highlight bar
      display.fillRect(0,y-1,128,9,SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
    } else {
      display.setTextColor(SSD1306_WHITE);
    }
    display.setCursor(2,y);
    display.print(getAppName(static_cast<AppId>(i)));
  }
  display.display();
}

void AppManager::drawActiveApp(Adafruit_SSD1306 &display, bool &timeSynced, void (*drawClock)(Adafruit_SSD1306&, bool&)) {
  switch(activeApp) {
    case AppId::CLOCK:
      if (drawClock) drawClock(display, timeSynced);
      break;
    case AppId::FITNESS:
      drawFitness(display);
      break;
    case AppId::TIMER:
      drawTimer(display);
      break;
    case AppId::STOCKS:
      drawStocks(display);
      break;
    case AppId::WEATHER:
      drawWeather(display);
      break;
    default:
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(0,0);
      display.println(F("Unknown app"));
      display.display();
  }
}

void AppManager::resetToClock() {
  activeApp = AppId::CLOCK;
  inMenu = false;
  menuIndex = static_cast<int>(AppId::CLOCK);
}
