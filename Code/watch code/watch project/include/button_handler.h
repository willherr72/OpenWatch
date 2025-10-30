#pragma once
#include <Arduino.h>

// Button configuration - GPIO pins from platformio.ini
#ifndef BUTTON_PRIMARY_PIN
#define BUTTON_PRIMARY_PIN 20
#endif

#ifndef MENU_BUTTON_PIN
#define MENU_BUTTON_PIN 21
#endif

#ifndef DEBOUNCE_TIME_MS
#define DEBOUNCE_TIME_MS 50
#endif

#ifndef LONG_PRESS_TIME_MS
#define LONG_PRESS_TIME_MS 1000
#endif

constexpr uint8_t BUTTON_PIN = BUTTON_PRIMARY_PIN;
constexpr uint32_t DEBOUNCE_TIME = DEBOUNCE_TIME_MS;
constexpr uint32_t LONG_PRESS_TIME = LONG_PRESS_TIME_MS;

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
  unsigned long lastDebugPrint = 0;
  
public:
  ButtonHandler(uint8_t buttonPin = BUTTON_PIN);
  void begin();
  ButtonEvent update();
  bool isButtonPressed() const;
  void enableWakeup();
  void printDebugStatus();
};