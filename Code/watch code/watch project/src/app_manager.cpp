#include "app_manager.h"
#include "fitness_app.h"
#include "timer_app.h"
#include "weather_app.h"
#include <climits>

AppManager::AppManager() : appCount(0), inMenu(false), menuJustClosed(false), menuDirty(false),
  menuBoundsValid(false), lastRenderedMenuIndex(-1), lastRenderedAppCount(-1), lastDrawnAppIndex(-1), activeAppIndex(0), menuIndex(0),
  touchOffsetX(0), touchOffsetY(0), touchScaleX(1.0f), touchScaleY(1.0f),
  lastTouchY(0), touchStartY(0), scrollStartIndex(0), touchScrollActive(false), touchStartTime(0), lastGestureTime(0) {}

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
  
  menuBoundsValid = false;
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
  
  // Reset touch scroll state
  touchScrollActive = false;
  lastTouchY = 0;
  touchStartY = 0;
  scrollStartIndex = menuIndex;
}

void AppManager::exitMenu() {
  inMenu = false;
  menuJustClosed = true;
  menuBoundsValid = false;
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
  menuBoundsValid = false;
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
    menuBoundsValid = false;
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
    display.setTextSize(3);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(name, 0, 0, &x1, &y1, &w, &h);

    int16_t baseline = startBaseline + i * lineSpacing;
    int16_t textX = (display.width() - static_cast<int16_t>(w)) / 2;
    if (textX < 0) textX = 0;

    int16_t rectPaddingX = 24;
    int16_t rectPaddingY = 18;
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

    int16_t centerX = rectX + rectW / 2;
    int16_t centerY = baseline - static_cast<int16_t>(h) / 2;
    int16_t measuredHalfW = rectW / 2;
    int16_t measuredHalfH = rectH / 2;
    int16_t expandedHalfW = measuredHalfW + 48;
    if (expandedHalfW < 70) {
      expandedHalfW = 70;
    }
    int16_t expandedHalfH = measuredHalfH + lineSpacing / 2;
    if (expandedHalfH < 42) {
      expandedHalfH = 42;
    }

    menuItemBounds[i] = {
      static_cast<int16_t>(rectX),
      static_cast<int16_t>(rectY),
      static_cast<int16_t>(rectW),
      static_cast<int16_t>(rectH),
      centerY,
      centerX,
      expandedHalfW,
      expandedHalfH
    };
  }
  menuDirty = false;
  lastRenderedMenuIndex = menuIndex;
  lastRenderedAppCount = appCount;
  menuBoundsValid = true;
}

bool AppManager::handleMenuTouch(int16_t x, int16_t y) {
  if (!inMenu || !menuBoundsValid) {
    return false;
  }

  float adjustedX = (static_cast<float>(x) + touchOffsetX) * touchScaleX;
  float adjustedY = (static_cast<float>(y) + touchOffsetY) * touchScaleY;
  int16_t ix = static_cast<int16_t>(adjustedX + 0.5f);
  int16_t iy = static_cast<int16_t>(adjustedY + 0.5f);

  // Find the closest app based primarily on Y-position (vertical distance)
  // This prevents jumping between apps when sliding vertically
  int closestIndex = -1;
  int16_t closestYDistance = 9999;
  
  for (int i = 0; i < appCount; i++) {
    const MenuItemBounds &bounds = menuItemBounds[i];
    
    // Calculate Y distance from touch to app center
    int16_t dy = abs(iy - bounds.centerY);
    
    // Check if touch is within reasonable X bounds (horizontal range)
    int16_t dx = abs(ix - bounds.centerX);
    if (dx > bounds.w) {
      continue;  // Touch is too far horizontally, skip this app
    }
    
    // Find the app with the smallest Y distance
    if (dy < closestYDistance) {
      closestYDistance = dy;
      closestIndex = i;
    }
  }
  
  // Update highlighted app if we found one
  // Use a reasonable Y threshold to avoid selecting apps that are too far away
  const int16_t MAX_Y_DISTANCE = 36;  // Half of line spacing
  
  if (closestIndex >= 0 && closestYDistance < MAX_Y_DISTANCE) {
    if (menuIndex != closestIndex) {
      menuIndex = closestIndex;
      menuDirty = true;
      Serial.printf("Touch highlight app %d (%s) - Y-dist=%d\n", 
                    menuIndex, apps[menuIndex].name, closestYDistance);
      return true;  // Menu changed, needs redraw
    }
  }
  
  return false;  // No change
}

bool AppManager::handleMenuGesture(uint8_t gesture) {
  if (!inMenu) {
    return false;
  }

  unsigned long now = millis();
  
  // Gesture codes from CST816T
  const uint8_t GESTURE_UP = 0x01;
  const uint8_t GESTURE_DOWN = 0x02;
  const uint8_t GESTURE_LEFT = 0x03;
  const uint8_t GESTURE_RIGHT = 0x04;
  const uint8_t GESTURE_CLICK = 0x05;
  const uint8_t GESTURE_DOUBLE_CLICK = 0x0B;
  
  // Cooldown for swipe gestures to slow down navigation
  const unsigned long SWIPE_COOLDOWN_MS = 175;  // 175ms between swipes
  
  Serial.printf("AppManager::handleMenuGesture called with gesture=0x%02X (inMenu=%d, timeSinceLastGesture=%lums)\n", 
                gesture, inMenu, now - lastGestureTime);
  
  switch (gesture) {
    case GESTURE_UP:
      // Swipe up = previous app (with cooldown)
      if (now - lastGestureTime >= SWIPE_COOLDOWN_MS) {
        previous();
        lastGestureTime = now;
        Serial.println("Gesture: Swipe UP - Previous app");
        return true;
      } else {
        Serial.printf("Gesture: Swipe UP blocked by cooldown (%lu ms remaining)\n", 
                      SWIPE_COOLDOWN_MS - (now - lastGestureTime));
        return false;
      }
      
    case GESTURE_DOWN:
      // Swipe down = next app (with cooldown)
      if (now - lastGestureTime >= SWIPE_COOLDOWN_MS) {
        next();
        lastGestureTime = now;
        Serial.println("Gesture: Swipe DOWN - Next app");
        return true;
      } else {
        Serial.printf("Gesture: Swipe DOWN blocked by cooldown (%lu ms remaining)\n", 
                      SWIPE_COOLDOWN_MS - (now - lastGestureTime));
        return false;
      }
      
    case GESTURE_DOUBLE_CLICK:
      // Double click not assigned
      Serial.printf("Gesture: Double Click (not assigned)\n");
      return false;
      
    case GESTURE_CLICK:
      // Single click not assigned
      Serial.println("Gesture: Single Click (not assigned)");
      return false;
      
    case GESTURE_LEFT:
    case GESTURE_RIGHT:
      // Horizontal swipes = select current app (NO COOLDOWN)
      Serial.printf("Gesture: Swipe %s - Selecting app %d (%s) - NO COOLDOWN\n", 
                    gesture == GESTURE_LEFT ? "LEFT" : "RIGHT", menuIndex, apps[menuIndex].name);
      select();
      lastGestureTime = now;  // Update for other gestures
      return true;
      
    default:
      return false;
  }
}

void AppManager::resetTouchScroll() {
  touchScrollActive = false;
  lastTouchY = 0;
  touchStartY = 0;
  scrollStartIndex = 0;
  touchStartTime = 0;
  // Don't reset lastGestureTime - keep cooldown between gestures
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
  menuBoundsValid = false;
}

void AppManager::markMenuDirty() {
  menuDirty = true;
}

void AppManager::setTouchCalibration(int16_t xOffset, int16_t yOffset, float xScale, float yScale) {
  touchOffsetX = xOffset;
  touchOffsetY = yOffset;
  touchScaleX = xScale;
  touchScaleY = yScale;
}
