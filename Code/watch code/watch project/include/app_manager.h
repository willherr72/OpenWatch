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
  
  bool inMenu;
  bool menuJustClosed;
  bool menuDirty;
  int lastRenderedMenuIndex;
  int lastRenderedAppCount;
  int lastDrawnAppIndex;
  int activeAppIndex;   // currently running app index
  int menuIndex;        // index while navigating menu
};
