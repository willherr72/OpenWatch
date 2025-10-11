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

// GC9A01 SPI pins remapped for ESP32-C3 SuperMini headers
constexpr int TFT_SCK  = 4;   // D4  -> FSPI CLK
constexpr int TFT_MOSI = 6;   // D6  -> FSPI MOSI (D pin)
constexpr int TFT_MISO = 5;   // D5  -> FSPI MISO (Q pin, not wired on panel)
constexpr int TFT_CS   = 7;   // D7  -> Chip select
constexpr int TFT_DC   = 8;   // D8  -> Data/Command select
constexpr int TFT_RST  = 9;   // D9  -> Reset line
constexpr int TFT_BL   = 10;  // D10 -> Backlight enable (set HIGH)

// Use Adafruit_GC9A01A over SPI
#include <SPI.h>
SPIClass displaySPI(FSPI);
Gc9Display display(&displaySPI, TFT_DC, TFT_CS, TFT_RST);

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
    true                   // isSpecial (this is the clock app)
  };
  
  appManager.registerApp(clockApp);
}

void setup() {
  Serial.begin(115200);
  while(!Serial && millis() < 1500) { }
  Serial.println();
  Serial.println(F("=== ESP32-C6 Boot ==="));
  Serial.println(F("Booting NTP clock..."));

  // Ensure maximum CPU frequency (ESP32-C6 max 160 MHz on Arduino core)
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
  appMgr.draw(display);
    delay(50); // modest refresh pacing
    return;
  }

  if (now - lastUpdate >= UPDATE_INTERVAL_MS) {
    lastUpdate = now;
  appMgr.drawActiveApp(display, timeSynced, drawCurrentTime);
  }
}