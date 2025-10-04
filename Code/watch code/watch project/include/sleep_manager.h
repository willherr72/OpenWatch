#pragma once
#include <Arduino.h>
#include <Adafruit_SSD1306.h>

enum class SleepState {  //Sleep state of esp
  AWAKE,
  GOING_TO_SLEEP,
  SLEEPING
};

class SleepManager {
private:
  SleepState state;
  
public:
  SleepManager();
  void triggerSleep();
  bool updateCountdown();
  void goToSleep(Adafruit_SSD1306 &display);
  void wakeUp();
  SleepState getState() const;
  void showWakeMessage(Adafruit_SSD1306 &display);
  bool handleWakeupReason();
};