#include "sleep_manager.h"
#include "esp_sleep.h"
#include "esp_log.h"

SleepManager::SleepManager() : state(SleepState::AWAKE) {
}

void SleepManager::triggerSleep() {
  if (state == SleepState::AWAKE) {
    // Go to sleep immediately (no countdown)
    state = SleepState::GOING_TO_SLEEP;
    Serial.println(F("Sleep triggered - going to sleep immediately"));
  }
}

bool SleepManager::updateCountdown() {
  if (state != SleepState::GOING_TO_SLEEP) {
    return false;
  }
  
  // No countdown - go to sleep immediately
  return true;
}

void SleepManager::goToSleep(Adafruit_GC9A01A &display) {
  Serial.println(F("Entering light sleep..."));
  
  // Power down display before sleep
  display.fillScreen(0x0000);  // Black
  
  // Turn off backlight if defined
  #ifdef TFT_BL_PIN
    digitalWrite(TFT_BL_PIN, LOW);
  #endif
  
  Serial.println(F("Display prepared for sleep"));
  
  // Configure wake-up sources for light sleep (only GPIO, no timer)
  esp_sleep_enable_gpio_wakeup();
  Serial.println(F("GPIO wake-up enabled for light sleep (button only)"));
  
  Serial.flush(); // Ensure message is sent before sleep
  
  state = SleepState::SLEEPING;
  
  // Small delay to ensure any pending operations complete
  delay(100);
  
  // Enter light sleep (only button can wake up)
  esp_light_sleep_start();
  
  // Code execution continues here after wake-up
  Serial.println(F("Woken up from light sleep!"));
  
  // Re-enable backlight
  #ifdef TFT_BL_PIN
    digitalWrite(TFT_BL_PIN, HIGH);
  #endif
  
  Serial.println(F("Display resumed after sleep"));
  
  // Update state
  state = SleepState::AWAKE;
}

void SleepManager::wakeUp() {
  state = SleepState::AWAKE;
  Serial.println(F("Wake up from sleep"));
}

SleepState SleepManager::getState() const {
  return state;
}

void SleepManager::showWakeMessage(Adafruit_GC9A01A &display) {
  display.fillScreen(0x0000);  // Black
  display.setTextColor(0xFFFF);  // White
  display.setTextSize(2);
  
  // Center "WAKE UP" message
  const char* msg = "WAKE UP";
  int16_t x = (display.width() - (strlen(msg) * 12)) / 2;
  int16_t y = (display.height() - 16) / 2;
  
  display.setCursor(x, y);
  display.print(msg);
}

bool SleepManager::handleWakeupReason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  
  Serial.printf("Wake-up cause: %d\n", wakeup_reason);
  
  switch(wakeup_reason) {
    case ESP_SLEEP_WAKEUP_GPIO:
      Serial.println(F("Wakeup from GPIO (button press)"));
      return true;
    case ESP_SLEEP_WAKEUP_TIMER:
      Serial.println(F("Wakeup from timer (10 second test wake-up)"));
      return true;
    case ESP_SLEEP_WAKEUP_UNDEFINED:
      Serial.println(F("Wakeup from reset/power on (not from sleep)"));
      return false;
    default:
      Serial.printf("Wakeup from other source: %d\n", wakeup_reason);
      return false;
  }
}