#pragma once
#include <Arduino.h>
#include "display_adapter.h"

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
  void goToSleep(Gc9Display &display);
  void wakeUp();
  SleepState getState() const;
  void showWakeMessage(Gc9Display &display);
  bool handleWakeupReason();
};