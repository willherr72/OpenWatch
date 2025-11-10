#pragma once
#include <Arduino.h>
#include <Adafruit_GC9A01A.h>

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
  void goToSleep(Adafruit_GC9A01A &display);
  void wakeUp();
  SleepState getState() const;
  void showWakeMessage(Adafruit_GC9A01A &display);
  bool handleWakeupReason();
};