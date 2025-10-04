#pragma once
#include <Arduino.h>

// Button configuration
constexpr uint8_t BUTTON_PIN = 20;  // GPIO20 on QT Py ESP32-C3 (available pin)
constexpr uint32_t DEBOUNCE_TIME_MS = 50;
constexpr uint32_t LONG_PRESS_TIME_MS = 1000;

enum class ButtonEvent {
  NONE,
  SHORT_PRESS,
  LONG_PRESS
};

class ButtonHandler {
private:
  uint8_t pin;
  bool lastState;
  bool currentState;
  unsigned long lastDebounceTime;
  unsigned long pressStartTime;
  bool isPressed;
  
public:
  ButtonHandler(uint8_t buttonPin = BUTTON_PIN);
  void begin();
  ButtonEvent update();
  bool isButtonPressed() const;
  void enableWakeup();
};