#include "clock_display.h"
#include <WiFi.h>
#include <time.h>
#include <cstring>

namespace {
bool clockInitialized = false;
bool clockPrevSynced = false;
char cachedTime[16] = "";
char cachedDate[24] = "";
char cachedOverlay[16] = "";
bool cachedOverlayActive = false;
bool cachedOverlayOutline = false;
int16_t cachedOverlayBottom = 0;
}

void resetClockDisplay(Gc9Display &display) {
  clockInitialized = false;
  clockPrevSynced = false;
  cachedTime[0] = '\0';
  cachedDate[0] = '\0';
  cachedOverlay[0] = '\0';
  cachedOverlayActive = false;
  cachedOverlayOutline = false;
  cachedOverlayBottom = 0;
  display.fillScreen(COLOR_BLACK);
}

// Use display.width()/height() from Gc9Display; no fixed constants here

// Attempt to obtain local time with timeout; returns true if successful
bool fetchLocalTime(struct tm &out, uint32_t timeoutMs) {
  time_t now; struct tm *tp;
  uint32_t start = millis();
  do {
    now = time(nullptr);
    if (now > 100000) { // plausible epoch
      // Manual timezone adjustment for Central Daylight Time (UTC-5)
      // October 8, 2025 is still in DST, so we subtract 5 hours (18000 seconds)
      now -= 18000;  // Convert UTC to CDT (UTC-5)
      tp = gmtime(&now);  // Use gmtime instead of localtime to avoid double timezone conversion
      if (tp) { out = *tp; return true; }
    }
    delay(50);
  } while (millis() - start < timeoutMs);
  return false;
}

// Draw current time (HH:MM:SS) or fallback uptime while waiting for NTP
void drawCurrentTime(Gc9Display &display, bool &timeSynced) {
  struct tm t = {};
  if (!timeSynced) {
    if (fetchLocalTime(t)) {
      timeSynced = true;
      Serial.println(F("Time synced successfully."));
      Serial.printf("Raw UTC time: %d:%02d:%02d\n", t.tm_hour, t.tm_min, t.tm_sec);
    }
  } else {
    if (!fetchLocalTime(t, 250)) { // quick poll
      timeSynced = false;
      Serial.println(F("Lost time sync; will retry."));
    }
  }

  auto clearRect = [&](int16_t rx, int16_t ry, int16_t rw, int16_t rh) {
    if (rw <= 0 || rh <= 0) return;
    if (rx < 0) {
      rw += rx;
      rx = 0;
    }
    if (ry < 0) {
      rh += ry;
      ry = 0;
    }
    if (rx >= display.width() || ry >= display.height()) return;
    if (rx + rw > display.width()) {
      rw = display.width() - rx;
    }
    if (ry + rh > display.height()) {
      rh = display.height() - ry;
    }
    if (rw <= 0 || rh <= 0) return;
    display.fillRect(rx, ry, rw, rh, COLOR_BLACK);
  };

  display.setTextColor(COLOR_WHITE);
  extern bool timerIsRunning();
  extern uint32_t timerRemainingSeconds();
  extern bool timerIsDone();

  bool overlayActive = false;
  bool overlayOutline = false;
  char overlayText[16] = "";

  if (timerIsRunning()) {
    uint32_t remain = timerRemainingSeconds();
    uint32_t m = remain / 60UL;
    uint32_t s = remain % 60UL;
    snprintf(overlayText, sizeof(overlayText), "Timer %lu:%02lu", (unsigned long)m, (unsigned long)s);
    overlayActive = true;
    overlayOutline = false;
  } else if (timerIsDone()) {
    strncpy(overlayText, "Timer Done", sizeof(overlayText) - 1);
    overlayText[sizeof(overlayText) - 1] = '\0';
    overlayActive = true;
    overlayOutline = true;
  }

  if (!timeSynced) {
    // Use same format as synced display, show RTC time while waiting for WiFi
    if (!clockInitialized) {
      display.fillScreen(COLOR_BLACK);
      clockInitialized = true;
    }

    display.setTextColor(COLOR_WHITE);
    
    // Get time from ESP32's RTC (starts at 12:00:00 on boot, increments automatically)
    time_t now = time(nullptr);
    struct tm *rtc_time = gmtime(&now);
    
    // Convert to 12-hour format
    int hour12 = rtc_time->tm_hour % 12;
    if (hour12 == 0) hour12 = 12;
    
    char timeBuf[16];
    snprintf(timeBuf, sizeof(timeBuf), "%2d:%02d:%02d", hour12, rtc_time->tm_min, rtc_time->tm_sec);
    
    display.setTextSize(3);
    int16_t x1,y1; uint16_t w,h; 
    display.getTextBounds(timeBuf,0,0,&x1,&y1,&w,&h);
    int16_t x = (display.width() - static_cast<int16_t>(w))/2;
    int16_t y = (display.height() - static_cast<int16_t>(h))/2;
    if (x < 0) x = 0;
    if (strcmp(timeBuf, cachedTime) != 0) {
      clearRect(x + x1 - 8, y + y1 - 6, static_cast<int16_t>(w) + 16, static_cast<int16_t>(h) + 12);
      display.setCursor(x, y);
      display.print(timeBuf);
      strncpy(cachedTime, timeBuf, sizeof(cachedTime) - 1);
      cachedTime[sizeof(cachedTime) - 1] = '\0';
    }
    
    // Display "no wifi" label above the time
    display.setTextSize(1);
    int16_t wx1, wy1;
    uint16_t ww, wh;
    display.getTextBounds("no wifi", 0, 0, &wx1, &wy1, &ww, &wh);
    int16_t wifiX = (display.width() - static_cast<int16_t>(ww)) / 2;
    int16_t wifiY = y - static_cast<int16_t>(wh) - 16;
    if (wifiX < 0) wifiX = 0;
    if (wifiY > 0) {
      clearRect(wifiX + wx1 - 4, wifiY + wy1 - 2, static_cast<int16_t>(ww) + 8, static_cast<int16_t>(wh) + 4);
      display.setCursor(wifiX, wifiY - wy1);
      display.print(F("no wifi"));
    }
    
    // Display date at bottom using RTC
    char dateBuf[24];
    strftime(dateBuf, sizeof(dateBuf), "%a %d", rtc_time);
    display.setTextSize(1);
    int16_t dx1, dy1; uint16_t dw, dh;
    display.getTextBounds(dateBuf, 0, 0, &dx1, &dy1, &dw, &dh);
    int16_t dateX = (display.width() - static_cast<int16_t>(dw)) / 2;
    int16_t dateY = display.height() - 24;
    if (dateX < 0) dateX = 0;
    if (strcmp(dateBuf, cachedDate) != 0) {
      clearRect(dateX + dx1 - 4, dateY + dy1 - 4, static_cast<int16_t>(dw) + 8, static_cast<int16_t>(dh) + 8);
      display.setCursor(dateX, dateY);
      display.print(dateBuf);
      strncpy(cachedDate, dateBuf, sizeof(cachedDate) - 1);
      cachedDate[sizeof(cachedDate) - 1] = '\0';
    }
    
    display.display();
    return;
  }

  if (!clockInitialized || !clockPrevSynced) {
    display.fillScreen(COLOR_BLACK);
    clockInitialized = true;
  }
  clockPrevSynced = true;

  if (!overlayActive && cachedOverlayActive) {
    clearRect(0, 8, display.width(), cachedOverlayBottom + 8);
    cachedOverlayActive = false;
    cachedOverlayOutline = false;
    cachedOverlayBottom = 0;
    cachedOverlay[0] = '\0';
  }

  if (overlayActive) {
    if (!cachedOverlayActive || overlayOutline != cachedOverlayOutline || strcmp(overlayText, cachedOverlay) != 0) {
      clearRect(0, 8, display.width(), (cachedOverlayBottom > 0 ? cachedOverlayBottom + 8 : 64));
      display.setTextSize(1);
      int16_t bx1, by1;
      uint16_t bw, bh;
      display.getTextBounds(overlayText, 0, 0, &bx1, &by1, &bw, &bh);
      int16_t paddingX = 12;
      int16_t paddingY = 6;
      int16_t pillW = static_cast<int16_t>(bw) + paddingX * 2;
      int16_t pillH = static_cast<int16_t>(bh) + paddingY * 2;
      int16_t pillX = (display.width() - pillW) / 2;
      int16_t pillY = 18;
      if (pillX < 0) pillX = 0;
      if (overlayOutline) {
        display.drawRoundRect(pillX, pillY, pillW, pillH, 10, COLOR_WHITE);
        display.setTextColor(COLOR_WHITE);
      } else {
        display.fillRoundRect(pillX, pillY, pillW, pillH, 10, COLOR_WHITE);
        display.setTextColor(COLOR_BLACK);
      }
      int16_t textX = pillX + paddingX;
      int16_t textY = pillY + paddingY - by1;
      display.setCursor(textX, textY);
      display.print(overlayText);
      display.setTextColor(COLOR_WHITE);
      cachedOverlayActive = true;
      cachedOverlayOutline = overlayOutline;
      cachedOverlayBottom = pillY + pillH;
      strncpy(cachedOverlay, overlayText, sizeof(cachedOverlay) - 1);
      cachedOverlay[sizeof(cachedOverlay) - 1] = '\0';
    }
  }

  int16_t overlayBottom = overlayActive ? cachedOverlayBottom : 0;
  if (!timeSynced) {
    unsigned long ms = millis();
    unsigned long totalSeconds = ms / 1000UL;
    uint16_t hours = (totalSeconds / 3600UL);
    uint8_t minutes = (totalSeconds / 60UL) % 60;
    uint8_t seconds = totalSeconds % 60;
    char buf[24];
    snprintf(buf, sizeof(buf), "UP %u:%02u:%02u", hours, minutes, seconds);
    display.setTextSize(1);
    display.setCursor(0,0); display.print(F("Waiting NTP"));
    display.setCursor(0,10); display.print(buf);
    display.setCursor(0,20);
    wl_status_t st = WiFi.status();
    switch(st) {
      case WL_CONNECTED: display.print(F("WiFi OK")); break;
      case WL_IDLE_STATUS: display.print(F("WiFi IDLE")); break;
      case WL_DISCONNECTED: display.print(F("WiFi DISC")); break;
      case WL_NO_SSID_AVAIL: display.print(F("SSID?")); break;
      case WL_CONNECT_FAILED: display.print(F("CONN FAIL")); break;
      default: display.print(F("WiFi ?")); break;
    }
    display.display();
    return;
  }

  // Convert to 12-hour format
  int hour12 = t.tm_hour % 12;
  if (hour12 == 0) hour12 = 12; // 0 becomes 12 (midnight/noon)
  
  char timeBuf[16];
  // Always show static colon
  snprintf(timeBuf, sizeof(timeBuf), "%2d:%02d:%02d", hour12, t.tm_min, t.tm_sec);
  char dateBuf[24]; 
  strftime(dateBuf, sizeof(dateBuf), "%a %d", &t); // %a = abbreviated day, %d = day of month
  
  // Check if WiFi is connected
  bool wifiConnected = (WiFi.status() == WL_CONNECTED);
  
  display.setTextSize(3);
  int16_t x1,y1; uint16_t w,h; display.getTextBounds(timeBuf,0,0,&x1,&y1,&w,&h);
  int16_t x = (display.width() - static_cast<int16_t>(w))/2;
  int16_t y = (display.height() - static_cast<int16_t>(h))/2;
  if (overlayBottom > 0 && y < overlayBottom + 12) {
    y = overlayBottom + 12;
  }
  if (x < 0) x = 0;
  if (strcmp(timeBuf, cachedTime) != 0) {
    clearRect(x + x1 - 8, y + y1 - 6, static_cast<int16_t>(w) + 16, static_cast<int16_t>(h) + 12);
    display.setCursor(x, y);
    display.print(timeBuf);
    strncpy(cachedTime, timeBuf, sizeof(cachedTime) - 1);
    cachedTime[sizeof(cachedTime) - 1] = '\0';
  }
  
  // Display "no wifi" label if not connected
  if (!wifiConnected) {
    display.setTextSize(1);
    int16_t wx1, wy1;
    uint16_t ww, wh;
    display.getTextBounds("no wifi", 0, 0, &wx1, &wy1, &ww, &wh);
    int16_t wifiX = (display.width() - static_cast<int16_t>(ww)) / 2;
    int16_t wifiY = y - static_cast<int16_t>(wh) - 16;  // Above the time
    if (wifiX < 0) wifiX = 0;
    if (wifiY > 0) {
      clearRect(wifiX + wx1 - 4, wifiY + wy1 - 2, static_cast<int16_t>(ww) + 8, static_cast<int16_t>(wh) + 4);
      display.setCursor(wifiX, wifiY - wy1);
      display.print(F("no wifi"));
    }
  } else {
    // Clear "no wifi" label if it was showing before (wipe the area)
    clearRect(0, 10, display.width(), 20);
  }
  
  // Date at bottom
  display.setTextSize(1);
  int16_t dx1, dy1; uint16_t dw, dh;
  display.getTextBounds(dateBuf, 0, 0, &dx1, &dy1, &dw, &dh);
  int16_t dateX = (display.width() - static_cast<int16_t>(dw)) / 2;
  int16_t dateY = display.height() - 24;
  if (dateX < 0) dateX = 0;
  if (strcmp(dateBuf, cachedDate) != 0) {
    clearRect(dateX + dx1 - 4, dateY + dy1 - 4, static_cast<int16_t>(dw) + 8, static_cast<int16_t>(dh) + 8);
    display.setCursor(dateX, dateY);
    display.print(dateBuf);
    strncpy(cachedDate, dateBuf, sizeof(cachedDate) - 1);
    cachedDate[sizeof(cachedDate) - 1] = '\0';
  }
  display.display();
}

// Show a simple message centered
void showMessage(Gc9Display &display, const __FlashStringHelper* msg) {
  display.clearDisplay();
  display.setTextColor(COLOR_WHITE);
  display.setTextSize(1);
  int16_t x1,y1; uint16_t w,h;
  display.getTextBounds((const char*)msg,0,0,&x1,&y1,&w,&h);
  int16_t x = (display.width() - w)/2;
  int16_t y = (display.height() - h)/2;
  display.setCursor(x,y);
  display.print(msg);
  display.display();
}

// Handle touch input on the clock display
void clockHandleTouch(const TouchPoint& touchPoint) {
  // Touch anywhere on the clock to toggle display on/off (wake)
  // Or handle swipe gestures
  
  if (touchPoint.gesture == TouchGesture::SWIPE_LEFT) {
    Serial.println("Clock: Swipe LEFT - could be used for other features");
  } else if (touchPoint.gesture == TouchGesture::SWIPE_UP) {
    Serial.println("Clock: Swipe UP - could be used for other features");
  } else if (touchPoint.gesture == TouchGesture::SWIPE_DOWN) {
    Serial.println("Clock: Swipe DOWN - could be used for other features");
  } else if (touchPoint.gesture == TouchGesture::SINGLE_CLICK) {
    Serial.println("Clock: Single click detected");
  } else if (touchPoint.touching && touchPoint.gesture == TouchGesture::NONE) {
    // Simple touch feedback - optional
    Serial.printf("Clock: Touch at x=%d, y=%d\n", touchPoint.x, touchPoint.y);
  }
}