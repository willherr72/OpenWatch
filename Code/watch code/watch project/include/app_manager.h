#pragma once
#include <Arduino.h>
#include "display_adapter.h"

// Forward declarations
typedef void (*AppDrawFunction)(Gc9Display &display);
typedef void (*AppUpdateFunction)();
typedef void (*AppButtonHandler)(int buttonEvent);
typedef void (*AppResetFunction)(Gc9Display &display);

// App structure for dynamic registration
struct App {
  const char* name;
  AppDrawFunction drawFunction;
  AppUpdateFunction updateFunction;  // Optional - can be nullptr
  AppButtonHandler buttonHandler;    // Optional - can be nullptr
  bool isSpecial;                   // true for clock app which has special handling
  AppResetFunction resetFunction;   // Optional - reset hook when app becomes active
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
  bool consumeMenuClosedFlag();
  void markMenuDirty();
  int currentAppIndex() const { return activeAppIndex; }
  const App* currentApp() const;
  void draw(Gc9Display &display);
  void drawActiveApp(Gc9Display &display, bool &timeSynced, void (*drawClock)(Gc9Display&, bool&));
  bool handleMenuTouch(int16_t x, int16_t y);
  bool handleMenuGesture(uint8_t gesture);
  void resetTouchScroll();
  void setTouchCalibration(int16_t xOffset, int16_t yOffset, float xScale, float yScale);
  
  // Dynamic app registration
  bool registerApp(const App& app);
  int getAppCount() const { return appCount; }
  const App* getApp(int index) const;
  
  // Force return to clock app and exit any active menu
  void resetToClock();
  
private:
  static const int MAX_APPS = 10;  // Maximum number of apps
  App apps[MAX_APPS];
  int appCount;

  struct MenuItemBounds {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int16_t centerY;
    int16_t centerX;
    int16_t halfW;
    int16_t halfH;
  };
  MenuItemBounds menuItemBounds[MAX_APPS];
  bool menuBoundsValid;
  int16_t touchOffsetX;
  int16_t touchOffsetY;
  float touchScaleX;
  float touchScaleY;
  
  // Touch scrolling state
  int16_t lastTouchY;
  int16_t touchStartY;
  int scrollStartIndex;
  bool touchScrollActive;
  unsigned long touchStartTime;
  unsigned long lastGestureTime;
  
  bool inMenu;
  bool menuJustClosed;
  bool menuDirty;
  int lastRenderedMenuIndex;
  int lastRenderedAppCount;
  int lastDrawnAppIndex;
  int activeAppIndex;   // currently running app index
  int menuIndex;        // index while navigating menu
};
