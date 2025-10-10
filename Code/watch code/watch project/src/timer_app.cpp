#include "timer_app.h"
#include "app_manager.h"

TimerAppContext gTimerCtx; // global context instance

namespace {
struct TimerRenderCache {
  bool initialized = false;
  TimerState lastState = TimerState::SELECTING;
  uint8_t lastPresetIndex = 0;
  uint32_t lastRemainingSeconds = UINT32_MAX;
};

TimerRenderCache renderCache;
}

static void formatTime(uint32_t msRemaining, char *buf, size_t len) {
  uint32_t totalSec = msRemaining / 1000UL;
  uint32_t m = totalSec / 60UL;
  uint32_t s = totalSec % 60UL;
  snprintf(buf, len, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
}

void resetTimerDisplay(Gc9Display &display) {
  renderCache = TimerRenderCache{};
  display.fillScreen(COLOR_BLACK);
}

void timerAppHandlePrimaryShort() {
  if (gTimerCtx.state == TimerState::SELECTING) {
    gTimerCtx.presetIndex = (gTimerCtx.presetIndex + 1) % 3;
    gTimerCtx.targetDuration = gTimerCtx.presets[gTimerCtx.presetIndex];
  }
}

void timerAppHandlePrimaryLong() {
  if (gTimerCtx.state == TimerState::SELECTING) {
    // start
    gTimerCtx.state = TimerState::RUNNING;
    gTimerCtx.startMillis = millis();
    // Capture epoch end if time() available
    time_t now = time(nullptr);
    if (now > 100000) {
      gTimerCtx.endEpoch = now + (gTimerCtx.targetDuration / 1000UL);
    } else {
      gTimerCtx.endEpoch = 0; // fallback to millis tracking
    }
  } else if (gTimerCtx.state == TimerState::DONE) {
    // reset to selecting
    gTimerCtx.state = TimerState::SELECTING;
    gTimerCtx.endEpoch = 0;
  }
}

void timerAppHandleMenuLong() {
  if (gTimerCtx.state == TimerState::RUNNING) {
    // cancel
    gTimerCtx.state = TimerState::SELECTING;
    gTimerCtx.endEpoch = 0;
  }
}

void timerAppUpdate() {
  if (gTimerCtx.state == TimerState::RUNNING) {
    bool done = false;
    if (gTimerCtx.endEpoch != 0) {
      time_t now = time(nullptr);
      if (now > 100000 && now >= gTimerCtx.endEpoch) {
        done = true;
      }
    } else {
      uint32_t elapsed = millis() - gTimerCtx.startMillis;
      if (elapsed >= gTimerCtx.targetDuration) done = true;
    }
    if (done) {
      gTimerCtx.state = TimerState::DONE;
    }
  }
}

bool timerIsRunning() { return gTimerCtx.state == TimerState::RUNNING; }
bool timerIsDone() { return gTimerCtx.state == TimerState::DONE; }
uint32_t timerRemainingSeconds() {
  if (gTimerCtx.state != TimerState::RUNNING) return 0;
  uint32_t remain = 0;
  if (gTimerCtx.endEpoch != 0) {
    time_t now = time(nullptr);
    if (now > 100000) {
      if (now >= gTimerCtx.endEpoch) {
        gTimerCtx.state = TimerState::DONE; // promote to done immediately
        gTimerCtx.endEpoch = 0; // optional clear
        return 0;
      } else {
        remain = (uint32_t)(gTimerCtx.endEpoch - now);
      }
    }
  }
  if (gTimerCtx.endEpoch == 0) {
    uint32_t elapsed = millis() - gTimerCtx.startMillis;
    if (elapsed >= gTimerCtx.targetDuration) {
      gTimerCtx.state = TimerState::DONE;
      return 0;
    }
    remain = (gTimerCtx.targetDuration - elapsed) / 1000UL;
  }
  return remain;
}

void drawTimer(Gc9Display &display) {
  timerAppUpdate();

  auto drawCentered = [&](const char *text, int16_t centerY, uint8_t size) {
    display.setTextSize(size);
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
    int16_t x = (display.width() - static_cast<int16_t>(w)) / 2;
    int16_t y = centerY - static_cast<int16_t>(h) / 2;
    if (x < 0) x = 0;
    display.setCursor(x, y - y1);
    display.print(text);
  };

  auto clearRegion = [&](int16_t x, int16_t y, int16_t w, int16_t h) {
    if (w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > display.width()) w = display.width() - x;
    if (y + h > display.height()) h = display.height() - y;
    if (w <= 0 || h <= 0) return;
    display.fillRect(x, y, w, h, COLOR_BLACK);
  };

  TimerState state = gTimerCtx.state;
  bool forceRedraw = !renderCache.initialized || state != renderCache.lastState;
  if (!renderCache.initialized) {
    renderCache.initialized = true;
  }

  switch (state) {
    case TimerState::SELECTING: {
      if (forceRedraw || renderCache.lastPresetIndex != gTimerCtx.presetIndex) {
        display.fillScreen(COLOR_BLACK);
        drawCentered("Timer", 40, 1);

        const char *labels[3] = { "1 min", "5 min", "10 min" };
        const int optionSpacing = 40;
        const int startY = 100;
        for (uint8_t i = 0; i < 3; ++i) {
          display.setTextSize(2);
          int16_t x1, y1; uint16_t w, h;
          display.getTextBounds(labels[i], 0, 0, &x1, &y1, &w, &h);
          int16_t textX = (display.width() - static_cast<int16_t>(w)) / 2;
          int16_t textY = startY + i * optionSpacing - static_cast<int16_t>(h) / 2;
          int16_t paddingX = 20;
          int16_t paddingY = 12;
          int16_t rectX = textX - paddingX;
          int16_t rectY = textY - paddingY / 2;
          int16_t rectW = static_cast<int16_t>(w) + paddingX * 2;
          int16_t rectH = static_cast<int16_t>(h) + paddingY;
          if (i == gTimerCtx.presetIndex) {
            display.fillRoundRect(rectX, rectY, rectW, rectH, 12, COLOR_WHITE);
            display.setTextColor(COLOR_BLACK);
          } else {
            display.drawRoundRect(rectX, rectY, rectW, rectH, 12, COLOR_WHITE);
            display.setTextColor(COLOR_WHITE);
          }
          display.setCursor(textX, textY - y1);
          display.print(labels[i]);
        }

        display.setTextColor(COLOR_WHITE);
        drawCentered("Short press: next", display.height() - 40, 1);
        drawCentered("Long press: start", display.height() - 22, 1);
        display.display();
      }
      renderCache.lastPresetIndex = gTimerCtx.presetIndex;
      renderCache.lastRemainingSeconds = UINT32_MAX;
      break;
    }
    case TimerState::RUNNING: {
      if (forceRedraw) {
        display.fillScreen(COLOR_BLACK);
        drawCentered("Timer", 40, 1);
        drawCentered("Menu hold: cancel", display.height() - 24, 1);
        renderCache.lastRemainingSeconds = UINT32_MAX;
      }

      auto computeRemainingMs = [&]() -> uint32_t {
        if (gTimerCtx.endEpoch != 0) {
          time_t now = time(nullptr);
          if (now > 100000 && now < gTimerCtx.endEpoch) {
            return static_cast<uint32_t>((gTimerCtx.endEpoch - now) * 1000UL);
          }
          if (now >= gTimerCtx.endEpoch && now > 100000) {
            return 0;
          }
        }
        uint32_t elapsed = millis() - gTimerCtx.startMillis;
        if (elapsed >= gTimerCtx.targetDuration) {
          return 0;
        }
        return gTimerCtx.targetDuration - elapsed;
      };

      uint32_t remainingMs = computeRemainingMs();
      uint32_t remainingSecs = remainingMs / 1000UL;
      if (remainingSecs != renderCache.lastRemainingSeconds || forceRedraw) {
        char buf[8];
        formatTime(remainingMs, buf, sizeof(buf));
        display.setTextSize(3);
        int16_t x1, y1; uint16_t w, h;
        display.getTextBounds(buf, 0, 0, &x1, &y1, &w, &h);
        int16_t x = (display.width() - static_cast<int16_t>(w)) / 2;
        int16_t y = (display.height() - static_cast<int16_t>(h)) / 2;
        int16_t padding = 12;
        clearRegion(x - padding, y - padding, static_cast<int16_t>(w) + padding * 2, static_cast<int16_t>(h) + padding * 2);
        display.setCursor(x, y - y1);
        display.print(buf);
        display.display();
        renderCache.lastRemainingSeconds = remainingSecs;
      }
      break;
    }
    case TimerState::DONE: {
      if (forceRedraw) {
        display.fillScreen(COLOR_BLACK);
        drawCentered("Timer", 40, 1);
        drawCentered("DONE", display.height() / 2, 3);
        drawCentered("Long press: reset", display.height() - 24, 1);
        display.display();
      }
      renderCache.lastRemainingSeconds = UINT32_MAX;
      break;
    }
  }

  renderCache.lastState = state;
}

void registerTimerApp(AppManager& appManager) {
  App timerApp = {
    "Timer",               // name
    drawTimer,             // drawFunction
    timerAppUpdate,        // updateFunction
    nullptr,               // buttonHandler (handled in main.cpp for now)
    false,                 // isSpecial (not the clock app)
    resetTimerDisplay      // resetFunction
  };
  
  appManager.registerApp(timerApp);
}
