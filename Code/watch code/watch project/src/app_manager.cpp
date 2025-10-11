#include "app_manager.h"
#include "fitness_app.h"
#include "timer_app.h"
#include "weather_app.h"

AppManager::AppManager() : appCount(0), inMenu(false), menuJustClosed(false), menuDirty(false),
  lastRenderedMenuIndex(-1), lastRenderedAppCount(-1), lastDrawnAppIndex(-1), activeAppIndex(0), menuIndex(0) {}

bool AppManager::registerApp(const App& app) {
  if (appCount >= MAX_APPS) {
    return false; // App registry full
  }
  
  apps[appCount] = app;
  appCount++;
  menuDirty = true;
  
  // If this is the first app or it's marked as special (clock), make it active
  if (appCount == 1 || app.isSpecial) {
    activeAppIndex = appCount - 1;
    menuIndex = activeAppIndex;
  }
  
  return true;
}

const App* AppManager::getApp(int index) const {
  if (index < 0 || index >= appCount) {
    return nullptr;
  }
  return &apps[index];
}

const App* AppManager::currentApp() const {
  return getApp(activeAppIndex);
}

void AppManager::enterMenu() {
  inMenu = true;
  menuJustClosed = false;
  menuIndex = activeAppIndex; // start highlight on current app
  menuDirty = true;
  lastRenderedMenuIndex = -1;
  lastRenderedAppCount = -1;
}

void AppManager::exitMenu() {
  inMenu = false;
  menuJustClosed = true;
}

void AppManager::next() {
  if (!inMenu) return;
  menuIndex = (menuIndex + 1) % appCount;
  menuDirty = true;
}

void AppManager::previous() {
  if (!inMenu) return;
  menuIndex = (menuIndex - 1 + appCount) % appCount;
  menuDirty = true;
}

void AppManager::select() {
  if (!inMenu) return;
  activeAppIndex = menuIndex;
  inMenu = false; // leave menu on selection
  menuJustClosed = true;
  lastDrawnAppIndex = -1;
}

bool AppManager::consumeMenuClosedFlag() {
  if (!menuJustClosed) {
    return false;
  }
  menuJustClosed = false;
  lastDrawnAppIndex = -1;
  return true;
}

void AppManager::draw(Gc9Display &display) {
  if (!menuDirty && lastRenderedMenuIndex == menuIndex && lastRenderedAppCount == appCount) {
    return;
  }

  display.clearDisplay();
  display.setTextColor(COLOR_WHITE);

  if (appCount == 0) {
    display.setTextSize(1);
    const char *msg = "No apps available";
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (display.width() - static_cast<int16_t>(w)) / 2;
    int16_t y = (display.height() - static_cast<int16_t>(h)) / 2;
    display.setCursor(x, y);
    display.print(msg);
    display.display();
    return;
  }

  auto drawCentered = [&](const char *text, int16_t topY, uint8_t size) {
    display.setTextSize(size);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (display.width() - static_cast<int16_t>(w)) / 2;
    if (x < 0) x = 0;
    display.setCursor(x, topY - y1);
    display.print(text);
    return static_cast<int16_t>(h);
  };

  const int16_t titleHeight = drawCentered("Menu", 28, 1);
  display.drawFastHLine(40, 28 + titleHeight + 4, display.width() - 80, COLOR_WHITE);

  const int lineSpacing = 36;
  const int topMargin = 64;
  const int bottomMargin = 48;
  const int16_t screenCenterY = display.height() / 2;
  const int totalSpan = (appCount > 1) ? (appCount - 1) * lineSpacing : 0;

  int16_t startBaseline = screenCenterY - totalSpan / 2;
  if (startBaseline < topMargin) startBaseline = topMargin;
  int16_t lastBaseline = startBaseline + totalSpan;
  int16_t maxLastBaseline = display.height() - bottomMargin;
  if (lastBaseline > maxLastBaseline) {
    int16_t shift = lastBaseline - maxLastBaseline;
    startBaseline -= shift;
    if (startBaseline < topMargin) startBaseline = topMargin;
  }

  for (int i = 0; i < appCount; ++i) {
    const char *name = apps[i].name;
    display.setTextSize(2);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(name, 0, 0, &x1, &y1, &w, &h);

    int16_t baseline = startBaseline + i * lineSpacing;
    int16_t textX = (display.width() - static_cast<int16_t>(w)) / 2;
    if (textX < 0) textX = 0;

    int16_t rectPaddingX = 18;
    int16_t rectPaddingY = 10;
    int16_t rectX = textX - rectPaddingX;
    int16_t rectY = baseline + y1 - rectPaddingY / 2;
    int16_t rectW = static_cast<int16_t>(w) + rectPaddingX * 2;
    int16_t rectH = static_cast<int16_t>(h) + rectPaddingY;

    if (rectX < 0) {
      rectW += rectX; // reduce width if it would overflow left
      rectX = 0;
    }
    if (rectX + rectW > display.width()) {
      rectW = display.width() - rectX;
    }
    if (rectY < titleHeight + 40) {
      rectY = titleHeight + 40;
    }
    if (rectY + rectH > display.height() - 20) {
      rectH = (display.height() - 20) - rectY;
    }
    if (rectH < 8) rectH = 8;
    if (rectW < 8) rectW = 8;
    if (rectY < 0) rectY = 0;

    if (i == menuIndex) {
      display.fillRoundRect(rectX, rectY, rectW, rectH, 12, COLOR_WHITE);
      display.setTextColor(COLOR_BLACK);
    } else {
      display.setTextColor(COLOR_WHITE);
    }

    display.setCursor(textX, baseline);
    display.print(name);
  }
  display.display();
  menuDirty = false;
  lastRenderedMenuIndex = menuIndex;
  lastRenderedAppCount = appCount;
}

void AppManager::drawActiveApp(Gc9Display &display, bool &timeSynced, void (*drawClock)(Gc9Display&, bool&)) {
  const App* app = currentApp();
  if (!app) {
    // No app registered - show error
    display.clearDisplay();
  display.setTextColor(COLOR_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(F("No apps"));
    display.display();
    return;
  }
  
  if (activeAppIndex != lastDrawnAppIndex) {
    if (app->resetFunction) {
      app->resetFunction(display);
    } else {
      display.fillScreen(COLOR_BLACK);
    }
    lastDrawnAppIndex = activeAppIndex;
  }

  // Special handling for clock app
  if (app->isSpecial && drawClock) {
    drawClock(display, timeSynced);
  } else if (app->drawFunction) {
    app->drawFunction(display);
  } else {
    // App has no draw function - show error
    display.clearDisplay();
  display.setTextColor(COLOR_WHITE);
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.print(F("App: "));
    display.println(app->name);
    display.println(F("No draw func"));
    display.display();
  }
}

void AppManager::resetToClock() {
  // Find the clock app (marked as special)
  for (int i = 0; i < appCount; i++) {
    if (apps[i].isSpecial) {
      activeAppIndex = i;
      menuIndex = i;
      break;
    }
  }
  inMenu = false;
  menuJustClosed = true;
  menuDirty = true;
  lastDrawnAppIndex = -1;
}

void AppManager::markMenuDirty() {
  menuDirty = true;
}
