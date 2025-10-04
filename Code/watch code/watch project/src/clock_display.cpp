#include "clock_display.h"
#include <WiFi.h>
#include <time.h>

// Display dimensions (local constants)
constexpr uint8_t SCREEN_WIDTH = 128;
constexpr uint8_t SCREEN_HEIGHT = 64;

// Attempt to obtain local time with timeout; returns true if successful
bool fetchLocalTime(struct tm &out, uint32_t timeoutMs) {
  time_t now; struct tm *tp;
  uint32_t start = millis();
  do {
    now = time(nullptr);
    if (now > 100000) { // plausible epoch
      tp = localtime(&now);
      if (tp) { out = *tp; return true; }
    }
    delay(50);
  } while (millis() - start < timeoutMs);
  return false;
}

// Draw current time (HH:MM:SS) or fallback uptime while waiting for NTP
void drawCurrentTime(Adafruit_SSD1306 &display, bool &timeSynced) {
  struct tm t = {};
  if (!timeSynced) {
    if (fetchLocalTime(t)) {
      timeSynced = true;
      Serial.println(F("Time synced successfully."));
    }
  } else {
    if (!fetchLocalTime(t, 250)) { // quick poll
      timeSynced = false;
      Serial.println(F("Lost time sync; will retry."));
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  // If timer running, show remaining at top small text
  extern bool timerIsRunning();
  extern uint32_t timerRemainingSeconds();
  uint8_t topOffset = 0;
  bool showTimerDone = false;
  extern bool timerIsDone();
  if (timerIsRunning()) {
    uint32_t remain = timerRemainingSeconds();
    char overlay[16];
    uint32_t m = remain / 60UL; uint32_t s = remain % 60UL;
    snprintf(overlay, sizeof(overlay), "T:%lu:%02lu", (unsigned long)m, (unsigned long)s);
    display.setTextSize(1);
    int16_t ox1, oy1; uint16_t ow, oh;
    display.getTextBounds(overlay, 0, 0, &ox1, &oy1, &ow, &oh);
    int16_t ox = (SCREEN_WIDTH - ow) / 2;
    if (ox < 0) ox = 0;
    display.setCursor(ox, 0);
    display.print(overlay);
    topOffset = 10; // keep prior vertical offset
  } else if (timerIsDone()) {
    // Show completion message
    const char *doneMsg = "Timer Completed";
    display.setTextSize(1);
    int16_t dx1, dy1; uint16_t dw, dh;
    display.getTextBounds(doneMsg, 0, 0, &dx1, &dy1, &dw, &dh);
    int16_t dx = (SCREEN_WIDTH - dw) / 2;
    display.setCursor(dx, 0);
    display.print(doneMsg);
    topOffset = 10;
    showTimerDone = true;
  }
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
  
  display.setTextSize(2);
  int16_t x1,y1; uint16_t w,h; display.getTextBounds(timeBuf,0,0,&x1,&y1,&w,&h);
  int16_t x = (SCREEN_WIDTH - w)/2; int16_t y = (SCREEN_HEIGHT - h)/2 - 4;
  display.setCursor(x,y - (topOffset ? 4 : 0)); display.print(timeBuf);
  
  // Date at bottom
  display.setTextSize(1);
  display.setCursor( (SCREEN_WIDTH - (int)strlen(dateBuf)*6)/2, SCREEN_HEIGHT - 10 );
  display.print(dateBuf);
  display.display();
}

// Show a simple message centered
void showMessage(Adafruit_SSD1306 &display, const __FlashStringHelper* msg) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  int16_t x1,y1; uint16_t w,h;
  display.getTextBounds((const char*)msg,0,0,&x1,&y1,&w,&h);
  int16_t x = (SCREEN_WIDTH - w)/2;
  int16_t y = (SCREEN_HEIGHT - h)/2;
  display.setCursor(x,y);
  display.print(msg);
  display.display();
}