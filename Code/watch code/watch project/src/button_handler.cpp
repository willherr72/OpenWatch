#include "button_handler.h"
#include "driver/gpio.h"
#include "esp_sleep.h"

ButtonHandler::ButtonHandler(uint8_t buttonPin) 
  : pin(buttonPin), lastState(HIGH), currentState(HIGH), 
    lastDebounceTime(0), pressStartTime(0), isPressed(false) {
}

void ButtonHandler::begin() {
  pinMode(pin, INPUT_PULLUP);  // Use internal pull-up resistor
  lastState = digitalRead(pin);
  currentState = lastState;
}

ButtonEvent ButtonHandler::update() {
  bool reading = digitalRead(pin);
  unsigned long currentTime = millis();
  
  // Debounce the button
  if (reading != lastState) {
    lastDebounceTime = currentTime;
  }
  
  if ((currentTime - lastDebounceTime) > DEBOUNCE_TIME_MS) {
    if (reading != currentState) {
      currentState = reading;
      
      if (currentState == LOW) {  // Button pressed (active low with pull-up)
        isPressed = true;
        pressStartTime = currentTime;
      } else if (isPressed) {  // Button released
        isPressed = false;
        unsigned long pressDuration = currentTime - pressStartTime;
        
        if (pressDuration >= LONG_PRESS_TIME_MS) {
          lastState = reading;
          return ButtonEvent::LONG_PRESS;
        } else {
          lastState = reading;
          return ButtonEvent::SHORT_PRESS;
        }
      }
    }
  }
  
  lastState = reading;
  return ButtonEvent::NONE;
}

bool ButtonHandler::isButtonPressed() const {
  return isPressed;
}

void ButtonHandler::enableWakeup() {
  // ESP32-C3 GPIO wake-up configuration for light sleep
  Serial.printf("Configuring light sleep wake-up on GPIO %d\n", pin);
  
  // Configure GPIO for wake-up (simpler config for light sleep)
  gpio_config_t config = {
    .pin_bit_mask = (1ULL << pin),
    .mode = GPIO_MODE_INPUT,
    .pull_up_en = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE
  };
  esp_err_t config_err = gpio_config(&config);
  
  // Set GPIO wake-up level (wake on LOW - when button is pressed)
  esp_err_t wakeup_err = gpio_wakeup_enable((gpio_num_t)pin, GPIO_INTR_LOW_LEVEL);
  
  Serial.printf("Light sleep GPIO setup - config: %s, wakeup_enable: %s\n", 
                config_err == ESP_OK ? "OK" : "FAILED",
                wakeup_err == ESP_OK ? "OK" : "FAILED");
  
  // Check current GPIO state
  int current_level = gpio_get_level((gpio_num_t)pin);
  Serial.printf("Current GPIO %d level: %d (should be 1 when button not pressed)\n", pin, current_level);
}