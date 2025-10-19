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

unsigned long lastDisplayUpdate = 0; // last display refresh
const unsigned long UPDATE_INTERVAL_MS = 500; // update twice per second
bool timeSynced = false;
unsigned long lastWiFiAttempt = 0;
unsigned long lastNtpAttempt = 0;
unsigned long lastWiFiCheckTime = 0;  // For throttling WiFi operations
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
  Serial.println(F("Starting up..."));

  // CPU already configured via board_build.f_cpu; log actual clock for confirmation
  Serial.printf("CPU Frequency currently %d MHz\n", getCpuFrequencyMhz());

  // Initialize buttons
  button.begin();
  menuButton.begin();
  
  // Debug: Check button GPIO state
  delay(100);
  Serial.printf("Initial button states - Primary (GPIO 20): %d, Menu (GPIO 21): %d\n", 
    digitalRead(20), digitalRead(21));
  
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

  // Register all apps dynamically
  registerClockApp(appMgr);
  registerFitnessApp(appMgr);
  registerTimerApp(appMgr);
  registerWeatherApp(appMgr);
  appMgr.setTouchCalibration(TOUCH_CALIB_X_OFFSET, TOUCH_CALIB_Y_OFFSET, TOUCH_CALIB_X_SCALE, TOUCH_CALIB_Y_SCALE);
  
  Serial.println(F("Registered apps with app manager"));
  
  // Initialize display to clock with "no wifi" state
  display.fillScreen(COLOR_BLACK);
  display.display();
  lastDisplayUpdate = 0; // force immediate update

  // Initialize WiFi in background (NON-BLOCKING)
  // WiFi will connect in the background while buttons remain responsive
  initWiFi(gmtOffsetSec, daylightOffsetSec, lastNtpAttempt);
  Serial.println(F("WiFi initialization started (non-blocking)"));
  
  // Initialize weather app
  weatherInit();
  
  Serial.println(F("Boot complete"));
}

void loop() {
  unsigned long now = millis();
  static unsigned long lastBackgroundTasks = 0;
  static unsigned long lastDisplayUpdate = 0;
  
  // ============================================
  // ABSOLUTE PRIORITY: Button handling ONLY
  // This runs EVERY iteration to catch all presses
  // ============================================
  ButtonEvent primaryEvent = button.update();
  ButtonEvent menuEvent = menuButton.update();

  // Handle menu button - respond to both SHORT and LONG presses to open menu
  if (menuEvent == ButtonEvent::SHORT_PRESS || menuEvent == ButtonEvent::LONG_PRESS) {
    if (appMgr.isMenuActive()) {
      if (menuEvent == ButtonEvent::SHORT_PRESS) {
        appMgr.exitMenu();
        touchResetState();
        lastDisplayUpdate = 0;
      }
    } else {
      // Open menu on any button press
      appMgr.enterMenu();
      touchResetState();
      lastDisplayUpdate = 0;
    }
  }

  // Menu navigation with primary button
  if (appMgr.isMenuActive()) {
    if (primaryEvent == ButtonEvent::SHORT_PRESS) {
      appMgr.next();
    } else if (primaryEvent == ButtonEvent::LONG_PRESS) {
      appMgr.select();
      lastDisplayUpdate = 0;
    }
    if (menuEvent == ButtonEvent::LONG_PRESS) {
      appMgr.previous();
    }
  } else {
    // App-specific button handling
    const App* currentApp = appMgr.currentApp();
    if (currentApp && strcmp(currentApp->name, "Timer") == 0) {
      extern void timerAppHandlePrimaryShort();
      extern void timerAppHandlePrimaryLong();
      extern void timerAppHandleMenuLong();
      if (primaryEvent == ButtonEvent::SHORT_PRESS) {
        timerAppHandlePrimaryShort();
        lastDisplayUpdate = 0;
      } else if (primaryEvent == ButtonEvent::LONG_PRESS) {
        timerAppHandlePrimaryLong();
        lastDisplayUpdate = 0;
      }
      if (menuEvent == ButtonEvent::LONG_PRESS) {
        timerAppHandleMenuLong();
        lastDisplayUpdate = 0;
      }
    } else {
      // Normal mode: long press primary -> sleep
      if (primaryEvent == ButtonEvent::LONG_PRESS) {
        Serial.println(F("Long press - sleep"));
        sleepMgr.triggerSleep();
      }
    }
  }
  
  // Sleep handling (blocking operation, but only when needed)
  if (sleepMgr.getState() == SleepState::GOING_TO_SLEEP) {
    if (sleepMgr.updateCountdown()) {
      sleepMgr.goToSleep(display);
      lastWiFiAttempt = 0;
      appMgr.resetToClock();
      sleepMgr.showWakeMessage(display);
      delay(2000);
      lastDisplayUpdate = 0;
      return;
    }
    return;
  }

  // ============================================
  // LOWER PRIORITY: Background tasks (less frequent)
  // ============================================
  if (now - lastBackgroundTasks > 100) {  // Every 100ms
    lastBackgroundTasks = now;
    
    // App updates
    const App* currentApp = appMgr.currentApp();
    if (currentApp && strcmp(currentApp->name, "Timer") == 0) {
      extern void timerAppUpdate();
      timerAppUpdate();
    } else if (currentApp && strcmp(currentApp->name, "Weather") == 0) {
      extern void weatherUpdate();
      weatherUpdate();
    }
    
    // WiFi/NTP background tasks (non-blocking)
    handleWiFiReconnection(now, lastWiFiAttempt);
    handleNTPRetry(now, timeSynced, gmtOffsetSec, daylightOffsetSec, lastNtpAttempt);
  }

  // ============================================
  // Display updates (controlled refresh rate)
  // ============================================
  if (now - lastDisplayUpdate >= UPDATE_INTERVAL_MS) {
    lastDisplayUpdate = now;
    
    // Menu rendering and touch handling
    if (appMgr.isMenuActive()) {
      TouchPoint touchPoint{};
      if (touchRead(touchPoint)) {
        bool handled = false;
        if (touchPoint.gesture != TouchGesture::NONE) {
          handled = appMgr.handleMenuGesture(static_cast<uint8_t>(touchPoint.gesture));
        }
        if (!handled && touchPoint.touching) {
          appMgr.handleMenuTouch(static_cast<int16_t>(touchPoint.x), static_cast<int16_t>(touchPoint.y));
          handled = true;
        }
        if (handled) {
          lastDisplayUpdate = 0;
          if (!appMgr.isMenuActive()) {
            touchResetState();
          }
        }
      } else {
        appMgr.resetTouchScroll();
      }
      
      if (appMgr.isMenuActive()) {
        appMgr.draw(display);
      }
    } else {
      // App rendering
      if (appMgr.consumeMenuClosedFlag()) {
        const App* current = appMgr.currentApp();
        if (current && current->isSpecial) {
          resetClockDisplay(display);
        } else {
          display.fillScreen(COLOR_BLACK);
        }
      }
      
      // Touch and gesture handling for active app
      TouchPoint touchPoint{};
      if (touchRead(touchPoint)) {
        // Check for right swipe gesture to open menu from any app
        if (touchPoint.gesture == TouchGesture::SWIPE_RIGHT) {
          Serial.println("Swipe RIGHT detected while in app - Opening menu");
          appMgr.enterMenu();
          touchResetState();
        } else {
          // Pass touch/gesture to specific app handlers
          const App* currentApp = appMgr.currentApp();
          if (currentApp) {
            if (strcmp(currentApp->name, "Timer") == 0) {
              extern void timerAppHandleTouch(const TouchPoint& touchPoint);
              timerAppHandleTouch(touchPoint);
            } else if (strcmp(currentApp->name, "Clock") == 0) {
              extern void clockHandleTouch(const TouchPoint& touchPoint);
              clockHandleTouch(touchPoint);
            }
          }
        }
      }
      
      appMgr.drawActiveApp(display, timeSynced, drawCurrentTime);
    }
  }
}