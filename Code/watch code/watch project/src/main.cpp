#include <Arduino.h>
#include <Wire.h>
#include "display_adapter.h"
#include <WiFi.h>
#include "secrets.h"
#include "wifi_handler.h"
#include "clock_display.h"
#include "button_handler.h"
#include "sleep_manager.h"
#include "app_manager.h"
#include "timer_app.h"
#include "weather_app.h"
#include "fitness_app.h"
#include "touch_input.h"
#include "calibration_app.h"

#ifndef TFT_SCK_PIN
#define TFT_SCK_PIN SCK
#endif
#ifndef TFT_MOSI_PIN
#define TFT_MOSI_PIN MOSI
#endif
#ifndef TFT_MISO_PIN
#define TFT_MISO_PIN MISO
#endif
#ifndef TFT_CS_PIN
#define TFT_CS_PIN SS
#endif
#ifndef TFT_DC_PIN
#define TFT_DC_PIN 8
#endif
#ifndef TFT_RST_PIN
#define TFT_RST_PIN 9
#endif
#ifndef TFT_BL_PIN
#define TFT_BL_PIN 10
#endif

constexpr int TFT_SCK  = TFT_SCK_PIN;
constexpr int TFT_MOSI = TFT_MOSI_PIN;
constexpr int TFT_MISO = TFT_MISO_PIN;
constexpr int TFT_CS   = TFT_CS_PIN;
constexpr int TFT_DC   = TFT_DC_PIN;
constexpr int TFT_RST  = TFT_RST_PIN;
constexpr int TFT_BL   = TFT_BL_PIN;

// Use Adafruit_GC9A01A over SPI
#include <SPI.h>
SPIClass displaySPI(FSPI);
Gc9Display display(&displaySPI, TFT_DC, TFT_CS, TFT_RST);

// Sleep and button management
#ifndef MENU_BUTTON_PIN
#define MENU_BUTTON_PIN 21
#endif

#ifndef BUTTON_PRIMARY_PIN
#define BUTTON_PRIMARY_PIN 0
#endif

#ifndef TOUCH_CALIB_X_OFFSET
#define TOUCH_CALIB_X_OFFSET 0
#endif
#ifndef TOUCH_CALIB_Y_OFFSET
#define TOUCH_CALIB_Y_OFFSET 0
#endif
#ifndef TOUCH_CALIB_X_SCALE
#define TOUCH_CALIB_X_SCALE 1.0f
#endif
#ifndef TOUCH_CALIB_Y_SCALE
#define TOUCH_CALIB_Y_SCALE 1.0f
#endif

ButtonHandler button(BUTTON_PRIMARY_PIN);               // primary button (sleep / select)
ButtonHandler menuButton(MENU_BUTTON_PIN); // second button for menu/navigation
SleepManager sleepMgr;
AppManager appMgr;

unsigned long lastUpdate = 0; // last display refresh
const unsigned long UPDATE_INTERVAL_MS = 500; // update twice per second
bool timeSynced = false;
unsigned long lastWiFiAttempt = 0;
unsigned long lastNtpAttempt = 0;
// Timezone configuration for Central Time Zone
// October 8, 2025 is during Daylight Saving Time (CDT = UTC-5 = -18000 seconds)
// DST ends first Sunday in November (Nov 2, 2025)
// Note: Using direct UTC-5 offset since automatic DST detection was unreliable
long gmtOffsetSec = -18000;     // CDT is UTC-5 (-5 * 3600 = -18000 seconds)  
int daylightOffsetSec = 0;      // Not using DST offset since we're setting CDT directly

// Register the clock app with the app manager
void registerClockApp(AppManager& appManager) {
  App clockApp = {
    "Clock",               // name
    nullptr,               // drawFunction (handled specially)
    nullptr,               // updateFunction (not needed)
    nullptr,               // buttonHandler (not needed)
    true,                  // isSpecial (this is the clock app)
    nullptr                // resetFunction (not needed for clock)
  };
  
  appManager.registerApp(clockApp);
}

void setup() {
  Serial.begin(115200);
  while(!Serial && millis() < 1500) { }
  Serial.println();
  Serial.println(F("=== ESP32-C6 Boot ==="));
  Serial.println(F("Booting NTP clock..."));

  // CPU already configured via board_build.f_cpu; log actual clock for confirmation
  Serial.printf("CPU Frequency currently %d MHz\n", getCpuFrequencyMhz());

  // Initialize buttons
  button.begin();
  menuButton.begin();
  touchInit();
  
  // Configure wake-up for light sleep
  Serial.println(F("Configuring wake-up for light sleep..."));
  button.enableWakeup(); // primary button wakes device
  // Optional: enable wake on menu button too
  menuButton.enableWakeup();
  Serial.println(F("Wake-up configuration complete"));

  // Initialize SPI bus and display
  displaySPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
  if (TFT_BL >= 0) {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
  }

  display.begin();
  display.setRotation(0); // 0-3 depending on mounting

  display.setTextColor(COLOR_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println(F("Registering Apps"));
  display.display();
  
  // Register all apps dynamically
  registerClockApp(appMgr);
  registerFitnessApp(appMgr);
  registerTimerApp(appMgr);
  registerWeatherApp(appMgr);
  registerCalibrationApp(appMgr);
  appMgr.setTouchCalibration(TOUCH_CALIB_X_OFFSET, TOUCH_CALIB_Y_OFFSET, TOUCH_CALIB_X_SCALE, TOUCH_CALIB_Y_SCALE);
  
  Serial.println(F("Registered apps with app manager"));
  
  display.setCursor(0, 10);
  display.println(F("WiFi Init"));
  display.display();
  lastUpdate = 0; // force immediate update

  showMessage(display, F("Connecting..."));
  initWiFi(gmtOffsetSec, daylightOffsetSec, lastNtpAttempt);
  
  // Initialize weather app
  weatherInit();
  
  if (isWiFiConnected()) {
    showMessage(display, F("Sync NTP"));
  } else {
    showMessage(display, F("No WiFi"));
  }
}

void loop() {
  unsigned long now = millis();
  
  // Update buttons
  ButtonEvent primaryEvent = button.update();
  ButtonEvent menuEvent = menuButton.update();

  // Enter / exit menu with menu button short press
  if (menuEvent == ButtonEvent::SHORT_PRESS) {
    if (appMgr.isMenuActive()) {
      // exit without selection
      appMgr.exitMenu();
      touchResetState();
      lastUpdate = 0; // refresh
    } else {
      appMgr.enterMenu();
      touchResetState();
    }
  }

  if (appMgr.isMenuActive()) {
    // Navigate menu: primary short -> next, long -> select; menu long -> previous
    if (primaryEvent == ButtonEvent::SHORT_PRESS) {
      appMgr.next();
    } else if (primaryEvent == ButtonEvent::LONG_PRESS) {
      appMgr.select();
      lastUpdate = 0;
    }
    if (menuEvent == ButtonEvent::LONG_PRESS) {
      appMgr.previous();
    }
  } else {
    // App-specific handling when not in menu
    const App* currentApp = appMgr.currentApp();
    if (currentApp && strcmp(currentApp->name, "Timer") == 0) {
      // Timer app button handling
      extern void timerAppHandlePrimaryShort();
      extern void timerAppHandlePrimaryLong();
      extern void timerAppHandleMenuLong();
      if (primaryEvent == ButtonEvent::SHORT_PRESS) {
        timerAppHandlePrimaryShort();
        lastUpdate = 0;
      } else if (primaryEvent == ButtonEvent::LONG_PRESS) {
        timerAppHandlePrimaryLong();
        lastUpdate = 0;
      }
      if (menuEvent == ButtonEvent::LONG_PRESS) {
        timerAppHandleMenuLong();
        lastUpdate = 0;
      }
    } else {
      // Normal mode: long press primary -> sleep
      if (primaryEvent == ButtonEvent::LONG_PRESS) {
        Serial.println(F("Long press detected - initiating sleep"));
        sleepMgr.triggerSleep();
      }
    }
  }
  
  // Handle sleep (immediate - no countdown)
  if (sleepMgr.getState() == SleepState::GOING_TO_SLEEP) {
    if (sleepMgr.updateCountdown()) {
      // Go to sleep immediately
      sleepMgr.goToSleep(display);
      // Execution continues here after wake-up from light sleep
      Serial.println(F("Returned from light sleep"));
      
      // Re-initialize WiFi after wake (it may have disconnected during sleep)
      Serial.println(F("Re-initializing WiFi after wake..."));
      lastWiFiAttempt = 0;  // Reset WiFi retry timer to allow immediate reconnect
      
      // Always return to clock app after waking
      appMgr.resetToClock();
      
      // Show wake message
      sleepMgr.showWakeMessage(display);
      delay(2000);  // Show wake message for 2 seconds
      
      // Force display update after wake
      lastUpdate = 0;
      return;
    }
    // No countdown display needed - sleep immediately
    return;
  }
  
  // Normal operation
  // Update all registered apps that have update functions
  for (int i = 0; i < appMgr.getAppCount(); i++) {
    const App* app = appMgr.getApp(i);
    if (app && app->updateFunction) {
      app->updateFunction();
    }
  }
  
  handleWiFiReconnection(now, lastWiFiAttempt);
  handleNTPRetry(now, timeSynced, gmtOffsetSec, daylightOffsetSec, lastNtpAttempt);

  if (!appMgr.isMenuActive()) {
    if (appMgr.consumeMenuClosedFlag()) {
      const App* current = appMgr.currentApp();
      if (current && current->isSpecial) {
        resetClockDisplay(display);
      } else {
        display.fillScreen(COLOR_BLACK);
      }
      lastUpdate = 0;
    }
  }

  if (appMgr.isMenuActive()) {
    TouchPoint touchPoint{};
    if (touchRead(touchPoint)) {
      static unsigned long lastTouchLog = 0;
      unsigned long nowMs = millis();
      
      // Log touch events (rate limited)
      if (nowMs - lastTouchLog > 500) {
        if (touchPoint.gesture != TouchGesture::NONE) {
          Serial.printf("Touch -> x:%u y:%u gesture:%u\n", touchPoint.x, touchPoint.y, static_cast<uint8_t>(touchPoint.gesture));
        } else {
          Serial.printf("Touch -> x:%u y:%u\n", touchPoint.x, touchPoint.y);
        }
        lastTouchLog = nowMs;
      }
      
      // Handle gestures first (swipe up/down for navigation, double tap for selection)
      bool handled = false;
      if (touchPoint.gesture != TouchGesture::NONE) {
        handled = appMgr.handleMenuGesture(static_cast<uint8_t>(touchPoint.gesture));
      }
      
      // If no gesture or gesture not handled, use touch position for highlighting
      if (!handled && touchPoint.touching) {
        appMgr.handleMenuTouch(static_cast<int16_t>(touchPoint.x), static_cast<int16_t>(touchPoint.y));
        // Touch always updates the display to show new highlight
        handled = true;
      }
      
      if (handled) {
        lastUpdate = 0;
        if (!appMgr.isMenuActive()) {
          touchResetState();
        }
      }
    } else {
      // No touch detected - reset scroll state
      appMgr.resetTouchScroll();
    }

    if (appMgr.isMenuActive()) {
      appMgr.draw(display);
      delay(50); // modest refresh pacing
      return;
    }
  }

  if (now - lastUpdate >= UPDATE_INTERVAL_MS) {
    lastUpdate = now;
  appMgr.drawActiveApp(display, timeSynced, drawCurrentTime);
  }
}