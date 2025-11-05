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
#include "altimeter_app.h"
#include "wifi_app.h"
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

// ═══════════════════════════════════════════════════════════════════════
// TIMEZONE CONFIGURATION - Change timezone here
// ═══════════════════════════════════════════════════════════════════════
// Central Standard Time (CST) = UTC-6 (DST ended Nov 2, 2025)
const long TIMEZONE_OFFSET_SEC = -21600;  // -6 hours * 3600 = -21600 seconds
// 
// Other common US timezones:
// CDT (Central Daylight Time) = -18000  (UTC-5)
// EST (Eastern Standard Time) = -18000  (UTC-5)  
// EDT (Eastern Daylight Time) = -14400  (UTC-4)
// PST (Pacific Standard Time) = -28800  (UTC-8)
// PDT (Pacific Daylight Time) = -25200  (UTC-7)
// ═══════════════════════════════════════════════════════════════════════

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
  Serial.println(F("=== ESP32-S3 Boot ==="));
  Serial.println(F("Starting up..."));

  // Set GPIO 17 to HIGH immediately
  pinMode(17, OUTPUT);
  digitalWrite(17, HIGH);
  Serial.println(F("GPIO 17 set to HIGH"));

  // CPU already configured via board_build.f_cpu; log actual clock for confirmation
  Serial.printf("CPU Frequency currently %d MHz\n", getCpuFrequencyMhz());

  // Initialize SPI bus and display EARLY (second thing after Serial/GPIO)
  Serial.println(F("Initializing display..."));
  displaySPI.begin(TFT_SCK, TFT_MISO, TFT_MOSI, TFT_CS);
  if (TFT_BL >= 0) {
    pinMode(TFT_BL, OUTPUT);
    digitalWrite(TFT_BL, HIGH);
  }

  display.begin();
  display.setRotation(0); // 0-3 depending on mounting
  display.fillScreen(COLOR_BLACK);
  display.display();
  Serial.println(F("Display initialized"));

  // Initialize I2C for sensors (BMP280, touch controller, etc.)
  Serial.println(F("Initializing I2C..."));
  Wire.begin(TOUCH_SDA_PIN, TOUCH_SCL_PIN);  // SDA=8, SCL=9 (shared with sensors)
  Serial.printf("I2C initialized on SDA=%d, SCL=%d\n", TOUCH_SDA_PIN, TOUCH_SCL_PIN);
  
  // Initialize WiFi after display for better stability
  initWiFi(TIMEZONE_OFFSET_SEC, 0, lastNtpAttempt);
  Serial.println(F("WiFi initialization started"));

  // Initialize buttons
  button.begin();
  menuButton.begin();
  
  // Debug: Check button GPIO state - use correct pin numbers from build flags
  delay(100);
  Serial.printf("Initial button states - Primary (GPIO %d): %d, Menu (GPIO %d): %d\n", 
    BUTTON_PRIMARY_PIN, digitalRead(BUTTON_PRIMARY_PIN), 
    MENU_BUTTON_PIN, digitalRead(MENU_BUTTON_PIN));
  
  touchInit();
  
  // Configure wake-up for light sleep
  Serial.println(F("Configuring wake-up for light sleep..."));
  button.enableWakeup(); // primary button wakes device
  // Optional: enable wake on menu button too
  menuButton.enableWakeup();
  Serial.println(F("Wake-up configuration complete"));

  // Register all apps dynamically
  registerClockApp(appMgr);
  registerAltimeterApp(appMgr);
  registerTimerApp(appMgr);
  registerWeatherApp(appMgr);
  registerWiFiApp(appMgr);
  appMgr.setTouchCalibration(TOUCH_CALIB_X_OFFSET, TOUCH_CALIB_Y_OFFSET, TOUCH_CALIB_X_SCALE, TOUCH_CALIB_Y_SCALE);
  
  Serial.println(F("Registered apps with app manager"));
  
  // Initialize weather app
  weatherInit();
  
  // Initialize WiFi app
  wifiAppInit();
  
  // Force immediate display update
  lastDisplayUpdate = 0;
  
  Serial.println(F("Boot complete"));
}

void loop() {
  unsigned long now = millis();
  static unsigned long lastBackgroundTasks = 0;
  static unsigned long lastDisplayUpdate = 0;
  static TouchGesture lastProcessedGesture = TouchGesture::NONE;  // Track last processed gesture to avoid repeats
  static unsigned long lastGestureTime = 0;  // Time of last processed gesture
  
  // ============================================
  // ABSOLUTE PRIORITY: Button handling ONLY
  // This runs EVERY iteration to catch all presses
  // ============================================
  ButtonEvent primaryEvent = button.update();
  ButtonEvent menuEvent = menuButton.update();

  // Debug button events
  if (primaryEvent != ButtonEvent::NONE) {
    Serial.printf("[BTN] Primary: %s\n", 
                  primaryEvent == ButtonEvent::SHORT_PRESS ? "SHORT" : "LONG");
  }
  if (menuEvent != ButtonEvent::NONE) {
    Serial.printf("[BTN] Menu: %s\n", 
                  menuEvent == ButtonEvent::SHORT_PRESS ? "SHORT" : "LONG");
  }

  // Handle menu button - SHORT press toggles menu, LONG press for navigation
  if (appMgr.isMenuActive()) {
    // Menu is active
    if (menuEvent == ButtonEvent::SHORT_PRESS) {
      Serial.println("[BTN] Exiting menu");
      appMgr.exitMenu();
      touchResetState();
      lastDisplayUpdate = 0;
    } else if (menuEvent == ButtonEvent::LONG_PRESS) {
      Serial.println("[BTN] Menu previous");
      appMgr.previous();
      lastDisplayUpdate = 0;
    }
    
    // Primary button navigates menu
    if (primaryEvent == ButtonEvent::SHORT_PRESS) {
      Serial.println("[BTN] Menu next");
      appMgr.next();
      lastDisplayUpdate = 0;
    } else if (primaryEvent == ButtonEvent::LONG_PRESS) {
      Serial.println("[BTN] Menu select");
      appMgr.select();
      lastDisplayUpdate = 0;
    }
  } else {
    // Menu is not active
    if (menuEvent == ButtonEvent::SHORT_PRESS || menuEvent == ButtonEvent::LONG_PRESS) {
      Serial.println("[BTN] Opening menu");
      appMgr.enterMenu();
      touchResetState();
      lastDisplayUpdate = 0;
    }
    
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
        Serial.println(F("[BTN] Long press - initiating sleep"));
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
  // CRITICAL: Touch input processing EVERY LOOP
  // This must run frequently to catch double-taps!
  // ============================================
  TouchPoint touchPoint{};
  bool touchReady = touchRead(touchPoint);
  
  if (touchReady) {
    // Only process a gesture once - don't repeat if gesture flag persists
    bool isNewGesture = (touchPoint.gesture != lastProcessedGesture) || 
                        (touchPoint.gesture == TouchGesture::NONE);
    
    if (touchPoint.gesture != TouchGesture::NONE) {
      lastProcessedGesture = touchPoint.gesture;
      lastGestureTime = now;
    } else if (now - lastGestureTime > 50) {
      // Clear the last gesture if no new input for 50ms
      lastProcessedGesture = TouchGesture::NONE;
    }
    
    if (isNewGesture && touchPoint.gesture != TouchGesture::NONE) {
      // New gesture detected - log it
      extern const char* gestureToString(TouchGesture gesture);
      Serial.printf("[GESTURE] %s at (%d, %d)\n", gestureToString(touchPoint.gesture), touchPoint.x, touchPoint.y);
      
      // Process gesture
      if (appMgr.isMenuActive()) {
        // Menu mode: handle menu gestures
        appMgr.handleMenuGesture(static_cast<uint8_t>(touchPoint.gesture));
        lastDisplayUpdate = 0;
      } else {
        // App mode: check for menu open gesture first
        if (touchPoint.gesture == TouchGesture::SWIPE_RIGHT) {
          Serial.println("Swipe RIGHT detected while in app - Opening menu");
          appMgr.enterMenu();
          touchResetState();
          lastDisplayUpdate = 0;
        } else {
          // Pass gesture to specific app handlers
          const App* currentApp = appMgr.currentApp();
          if (currentApp) {
            if (strcmp(currentApp->name, "Timer") == 0) {
              extern void timerAppHandleTouch(const TouchPoint& touchPoint);
              timerAppHandleTouch(touchPoint);
              lastDisplayUpdate = 0;
            } else if (strcmp(currentApp->name, "Clock") == 0) {
              extern void clockHandleTouch(const TouchPoint& touchPoint);
              clockHandleTouch(touchPoint);
            }
          }
        }
      }
    } else if (touchPoint.gesture == TouchGesture::NONE && touchPoint.touching) {
      // No gesture, but touching - handle continuous touch for menu navigation
      if (appMgr.isMenuActive()) {
        appMgr.handleMenuTouch(static_cast<int16_t>(touchPoint.x), static_cast<int16_t>(touchPoint.y));
      }
    }
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
    }
    
    // Weather updates - run in background regardless of active app
    extern void weatherUpdate();
    weatherUpdate();
    
    // WiFi/NTP background tasks (non-blocking)
    handleWiFiReconnection(now, lastWiFiAttempt);
    handleNTPRetry(now, timeSynced, TIMEZONE_OFFSET_SEC, 0, lastNtpAttempt);
  }

  // ============================================
  // Display updates (controlled refresh rate)
  // ============================================
  if (now - lastDisplayUpdate >= UPDATE_INTERVAL_MS) {
    lastDisplayUpdate = now;
    
    // Menu rendering
    if (appMgr.isMenuActive()) {
      appMgr.resetTouchScroll();
      appMgr.draw(display);
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
      
      appMgr.drawActiveApp(display, timeSynced, drawCurrentTime);
    }
    
    display.display();
  }
}