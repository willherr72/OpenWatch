#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include "secrets.h"
#include "wifi_handler.h"
#include "clock_display.h"
#include "button_handler.h"
#include "sleep_manager.h"
#include "app_manager.h"
#include "timer_app.h"
#include "weather_app.h"

// Display dimensions
const uint8_t SCREEN_WIDTH = 128;
const uint8_t SCREEN_HEIGHT = 64; // 64 or 32 depending on module

// I2C pins (QT Py ESP32-C3 default: SDA = 5, SCL = 6)
constexpr int I2C_SDA = 8;
constexpr int I2C_SCL = 9;

// I2C address for SSD1306 (0x3C common, 0x3D alternative)
constexpr uint8_t OLED_ADDR = 0x3C;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Sleep and button management
ButtonHandler button;               // primary button (sleep / select)
constexpr uint8_t BUTTON_MENU_PIN = 21; // choose a free GPIO for menu button (adjust wiring)
ButtonHandler menuButton(BUTTON_MENU_PIN); // second button for menu/navigation
SleepManager sleepMgr;
AppManager appMgr;

unsigned long lastUpdate = 0; // last display refresh
const unsigned long UPDATE_INTERVAL_MS = 500; // update twice per second
bool timeSynced = false;
unsigned long lastWiFiAttempt = 0;
const unsigned long WIFI_RETRY_INTERVAL = 15000; // 15s to reduce churn
unsigned long lastNtpAttempt = 0;
const unsigned long NTP_RETRY_INTERVAL = 20000; // 20s between configTime attempts
unsigned long firstBootMillis = 0;
// Timezone configuration for CST (Central Standard Time, UTC-6)
long gmtOffsetSec = -21600;     // CST is UTC-6 (-6 * 3600 = -21600 seconds)
int daylightOffsetSec = 3600;   // CDT adds 1 hour during daylight saving time

void setup() {
  Serial.begin(115200);
  while(!Serial && millis() < 1500) { }
  Serial.println();
  Serial.println(F("=== ESP32-C3 Boot ==="));
  Serial.println(F("Booting NTP clock..."));

  // Ensure maximum CPU frequency (ESP32-C3 max 160 MHz)
  setCpuFrequencyMhz(160);
  delay(10);
  Serial.printf("CPU Frequency set to %d MHz\n", getCpuFrequencyMhz());

  // Initialize buttons
  button.begin();
  menuButton.begin();
  
  // Configure wake-up for light sleep
  Serial.println(F("Configuring wake-up for light sleep..."));
  button.enableWakeup(); // primary button wakes device
  // Optional: enable wake on menu button too
  menuButton.enableWakeup();
  Serial.println(F("Wake-up configuration complete"));
// Initialize I2C explicitly with selected pins (some cores support Wire.setPins)
#if defined(ARDUINO_ARCH_ESP32)
  Wire.begin(I2C_SDA, I2C_SCL);
#else
  Wire.begin();
#endif

  // Initialize display
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) {
    // Display init failed; halt (could add retry or serial log)
    while (true) {
      delay(1000);
    }
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(0, 0);
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
  firstBootMillis = millis();
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
      lastUpdate = 0; // refresh
    } else {
      appMgr.enterMenu();
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
    if (appMgr.currentApp() == AppId::TIMER) {
      // Include timer app header
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
  // Update weather app fetch state
  weatherUpdate();
  handleWiFiReconnection(now, lastWiFiAttempt);
  handleNTPRetry(now, timeSynced, gmtOffsetSec, daylightOffsetSec, lastNtpAttempt);

  if (appMgr.isMenuActive()) {
    appMgr.draw(display);
    delay(50); // modest refresh pacing
    return;
  }

  if (now - lastUpdate >= UPDATE_INTERVAL_MS) {
    lastUpdate = now;
    appMgr.drawActiveApp(display, timeSynced, drawCurrentTime);
  }
}