#include "timer_app.h"

TimerAppContext gTimerCtx; // global context instance

static void formatTime(uint32_t msRemaining, char *buf, size_t len) {
  uint32_t totalSec = msRemaining / 1000UL;
  uint32_t m = totalSec / 60UL;
  uint32_t s = totalSec % 60UL;
  snprintf(buf, len, "%02lu:%02lu", (unsigned long)m, (unsigned long)s);
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

void drawTimer(Adafruit_SSD1306 &display) {
  timerAppUpdate();
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);

  if (gTimerCtx.state == TimerState::SELECTING) {
    display.setTextSize(1);
    display.setCursor(0,0);
    display.println(F("Timer"));
    display.drawLine(0,10,127,10,SSD1306_WHITE);
    // Show presets
    const char *labels[3] = { "1 min", "5 min", "10 min" };
    for (uint8_t i=0;i<3;i++) {
      int y = 14 + i*10;
      if (i == gTimerCtx.presetIndex) {
        display.fillRect(0,y-1,128,9,SSD1306_WHITE);
        display.setTextColor(SSD1306_BLACK);
      } else {
        display.setTextColor(SSD1306_WHITE);
      }
      display.setCursor(2,y);
      display.print(labels[i]);
    }
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 56);
    display.setTextSize(1);
    display.print(F("Short: cycle  Long: start"));
  } else if (gTimerCtx.state == TimerState::RUNNING) {
    uint32_t remainingMs;
    if (gTimerCtx.endEpoch != 0) {
      time_t now = time(nullptr);
      if (now > 100000 && now < gTimerCtx.endEpoch) {
        remainingMs = (uint32_t)( (gTimerCtx.endEpoch - now) * 1000UL );
      } else if (now >= gTimerCtx.endEpoch && now > 100000) {
        remainingMs = 0;
      } else {
        uint32_t elapsed = millis() - gTimerCtx.startMillis;
        remainingMs = (elapsed >= gTimerCtx.targetDuration) ? 0 : (gTimerCtx.targetDuration - elapsed);
      }
    } else {
      uint32_t elapsed = millis() - gTimerCtx.startMillis;
      remainingMs = (elapsed >= gTimerCtx.targetDuration) ? 0 : (gTimerCtx.targetDuration - elapsed);
    }
    char buf[8];
    formatTime(remainingMs, buf, sizeof(buf));
    display.setTextSize(2);
    int16_t x1,y1; uint16_t w,h;
    display.getTextBounds(buf,0,0,&x1,&y1,&w,&h);
    int16_t x = (display.width()-w)/2;
    int16_t y = (display.height()-h)/2;
    display.setCursor(x,y);
    display.print(buf);
    display.setTextSize(1);
    display.setCursor(0,0);
    display.print(F("Timer"));
    display.setCursor(0,56);
    display.print(F("Menu long: cancel"));
  } else if (gTimerCtx.state == TimerState::DONE) {
    display.setTextSize(2);
    const char *doneMsg = "DONE";
    int16_t x1,y1; uint16_t w,h;
    display.getTextBounds(doneMsg,0,0,&x1,&y1,&w,&h);
    int16_t x = (display.width()-w)/2;
    int16_t y = (display.height()-h)/2;
    display.setCursor(x,y);
    display.print(doneMsg);
    display.setTextSize(1);
    display.setCursor(0,0);
    display.print(F("Timer"));
    display.setCursor(0,56);
    display.print(F("Long: reset"));
  }
  display.display();
}
