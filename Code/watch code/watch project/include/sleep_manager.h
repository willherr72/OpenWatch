#pragma once
#include <Arduino.h>
#include <Adafruit_SSD1306.h>

enum class SleepState {
  AWAKE,
  GOING_TO_SLEEP,
  SLEEPING
};

class SleepManager {
private:
  SleepState state;
  unsigned long sleepCountdownStart;
  static constexpr unsigned long SLEEP_COUNTDOWN_MS = 3000; // 3 second countdown
  
public:
  SleepManager();
  void triggerSleep();
  bool updateCountdown();
  unsigned long getRemainingCountdownMs() const;
  void goToSleep(Adafruit_SSD1306 &display);
  void wakeUp();
  SleepState getState() const;
  void showSleepCountdown(Adafruit_SSD1306 &display, unsigned long remainingMs);
  void showWakeMessage(Adafruit_SSD1306 &display);
  bool handleWakeupReason();
};