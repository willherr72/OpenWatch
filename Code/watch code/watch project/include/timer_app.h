#pragma once
#include "display_adapter.h"
#include "touch_input.h"
#include <Arduino.h>

// Forward declaration
class AppManager;

// Simple timer app with preset selection and countdown.
// Interaction model (using existing buttons):
//  - Short press primary: cycle preset (when not running and not done)
//  - Long press primary: start timer (when in select state) or reset (when done)
//  - Short press menu: exit to clock (handled by menu system outside) or ignored when running
//  - Long press menu: cancel running timer back to select state
// States: SELECTING, RUNNING, DONE

enum class TimerState : uint8_t { SELECTING, RUNNING, DONE };

struct TimerAppContext {
  TimerState state = TimerState::SELECTING;
  // Preset durations in milliseconds
  const uint32_t presets[3] = { 1UL*60UL*1000UL, 5UL*60UL*1000UL, 10UL*60UL*1000UL };
  uint8_t presetIndex = 0;
  uint32_t startMillis = 0;
  uint32_t targetDuration = presets[0];
  // Persistence support: store absolute end epoch (seconds since 1970) when started.
  // 0 means not set / not running.
  time_t endEpoch = 0;
};

extern TimerAppContext gTimerCtx;

void timerAppHandlePrimaryShort();
void timerAppHandlePrimaryLong();
void timerAppHandleMenuLong();
void timerAppHandleTouch(const TouchPoint& touchPoint);
void timerAppUpdate();
void drawTimer(Gc9Display &display);
void resetTimerDisplay(Gc9Display &display);
// Query helpers for overlay
bool timerIsRunning();
bool timerIsDone();
uint32_t timerRemainingSeconds();

// Register this app with the app manager
void registerTimerApp(AppManager& appManager);
