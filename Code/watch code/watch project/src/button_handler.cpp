#include "button_handler.h"
#include "driver/gpio.h"
#include "esp_sleep.h"

ButtonHandler::ButtonHandler(uint8_t buttonPin) 
  : pin(buttonPin), lastState(LOW), currentState(LOW), 
    lastDebounceTime(0), pressStartTime(0), isPressed(false) {
}

void ButtonHandler::begin() {
  // Buttons are active HIGH (pulled low by default, go high when pressed)
  // Do NOT use INPUT_PULLUP since buttons are already pulled down externally
  pinMode(pin, INPUT);
  delay(100);  // Let GPIO settle
  
  // Read initial state
  lastState = digitalRead(pin);
  currentState = lastState;
  
  // Explicit serial output with diagnostics
  Serial.flush();
  Serial.printf("[BTN] Initializing button on GPIO %d\n", (int)pin);
  Serial.printf("[BTN] Initial state: %d (0=not pressed/released, 1=pressed)\n", lastState);
  Serial.printf("[BTN] Button is active HIGH (external pull-down)\n");
  
  // If GPIO is reading HIGH on startup, button might be stuck pressed
  if (lastState == HIGH) {
    Serial.printf("[BTN] WARNING: GPIO %d is reading HIGH on startup!\n", (int)pin);
    Serial.println("[BTN] This could indicate button is physically pressed");
  }
  Serial.flush();
}

ButtonEvent ButtonHandler::update() {
  bool reading = digitalRead(pin);
  unsigned long currentTime = millis();
  
  // Debounce the button
  if (reading != lastState) {
    lastDebounceTime = currentTime;
    lastState = reading;
    return ButtonEvent::NONE;  // Bounce detected, ignore
  }
  
  // Check if debounce time has elapsed
  if ((currentTime - lastDebounceTime) > DEBOUNCE_TIME_MS) {
    if (reading != currentState) {
      currentState = reading;
      
      if (currentState == HIGH) {  // Button pressed (active high)
        isPressed = true;
        pressStartTime = currentTime;
        return ButtonEvent::NONE;  // Press detected, wait for release
      } else if (isPressed) {  // Button released (went back to LOW)
        isPressed = false;
        unsigned long pressDuration = currentTime - pressStartTime;
        
        if (pressDuration >= LONG_PRESS_TIME_MS) {
          return ButtonEvent::LONG_PRESS;
        } else {
          return ButtonEvent::SHORT_PRESS;
        }
      }
    }
  }
  
  return ButtonEvent::NONE;
}

bool ButtonHandler::isButtonPressed() const {
  return isPressed;
}

void ButtonHandler::printDebugStatus() {
  unsigned long now = millis();
  if ((now - lastDebugPrint) > 2000) {  // Print every 2 seconds
    lastDebugPrint = now;
    Serial.printf("[BTN-DEBUG GPIO%d] reading=%d lastState=%d currentState=%d isPressed=%d\n", 
                  pin, digitalRead(pin), lastState, currentState, isPressed);
  }
}

void ButtonHandler::enableWakeup() {
  // ESP32-S3 GPIO wake-up configuration for light sleep
  // Buttons are active HIGH (pulled low externally, go high when pressed)
  Serial.printf("Configuring light sleep wake-up on GPIO %d (active HIGH)\n", pin);
  
  // Configure GPIO for wake-up (simple input, no pull-up needed)
  gpio_config_t config = {
    .pin_bit_mask = (1ULL << pin),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_DISABLE,      // No internal pull-up (external pull-down)
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  esp_err_t config_err = gpio_config(&config);
  
  // Set GPIO wake-up level (wake on HIGH - when button is pressed)
  esp_err_t wakeup_err = gpio_wakeup_enable((gpio_num_t)pin, GPIO_INTR_HIGH_LEVEL);
  
  Serial.printf("Light sleep GPIO setup - config: %s, wakeup_enable: %s\n", 
                config_err == ESP_OK ? "OK" : "FAILED",
                wakeup_err == ESP_OK ? "OK" : "FAILED");
  
  // Check current GPIO state
  int current_level = gpio_get_level((gpio_num_t)pin);
  Serial.printf("Current GPIO %d level: %d (should be 0 when button not pressed)\n", pin, current_level);
}