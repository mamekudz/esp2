#include "display_power.h"

#include <stdarg.h>

namespace {

void log_display_power(const char *fmt, ...) {
  char buf[160];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.printf("[DISPLAY-POWER] %s\n", buf);
}

constexpr uint32_t kMin1Ms = 60UL * 1000UL;
constexpr uint32_t kMin2Ms = 2UL * 60UL * 1000UL;
constexpr uint32_t kMin5Ms = 5UL * 60UL * 1000UL;
constexpr uint32_t kMin10Ms = 10UL * 60UL * 1000UL;

} // namespace

uint32_t display_power_screensaver_ms(const DisplayPowerSettings &s) {
  if (s.screensaver_override_ms != 0) {
    return s.screensaver_override_ms;
  }
  switch (s.screensaver) {
  case ScreensaverTimeout::Off:
    return 0;
  case ScreensaverTimeout::Min1:
    return kMin1Ms;
  case ScreensaverTimeout::Min2:
    return kMin2Ms;
  case ScreensaverTimeout::Min5:
    return kMin5Ms;
  }
  return kMin2Ms;
}

uint32_t display_power_screen_off_ms(const DisplayPowerSettings &s) {
  if (s.screen_off_override_ms != 0) {
    return s.screen_off_override_ms;
  }
  switch (s.screen_off) {
  case ScreenOffTimeout::Never:
    return 0;
  case ScreenOffTimeout::Min2:
    return kMin2Ms;
  case ScreenOffTimeout::Min5:
    return kMin5Ms;
  case ScreenOffTimeout::Min10:
    return kMin10Ms;
  }
  return kMin5Ms;
}

const char *DisplayPowerManager::stateName(DisplayPowerState s) {
  switch (s) {
  case DisplayPowerState::Active:
    return "ACTIVE";
  case DisplayPowerState::Screensaver:
    return "SCREENSAVER";
  case DisplayPowerState::Off:
    return "OFF";
  }
  return "?";
}

void DisplayPowerManager::logTransition(DisplayPowerState from,
                                        DisplayPowerState to,
                                        const char *reason) {
  if (reason != nullptr && reason[0] != '\0') {
    log_display_power("%s -> %s reason=%s", stateName(from), stateName(to),
                      reason);
  } else {
    log_display_power("%s -> %s", stateName(from), stateName(to));
  }
}

void DisplayPowerManager::begin(DrawScreensaverFn draw_ss,
                                RestoreUiFn restore_ui,
                                PanelSleepFn panel_sleep,
                                PanelWakeFn panel_wake) {
  draw_ss_ = draw_ss;
  restore_ui_ = restore_ui;
  panel_sleep_ = panel_sleep;
  panel_wake_ = panel_wake;
  state_ = DisplayPowerState::Active;
  last_activity_ms_ = millis();
  last_ss_move_ms_ = last_activity_ms_;
  ss_slot_ = 0;
  begun_ = true;
}

void DisplayPowerManager::setSettings(const DisplayPowerSettings &settings) {
  settings_ = settings;
}

bool DisplayPowerManager::notifyActivity(const char *reason) {
  if (!begun_) {
    return false;
  }
  last_activity_ms_ = millis();
  if (state_ == DisplayPowerState::Active) {
    return false;
  }
  enterActive(reason != nullptr ? reason : "activity");
  return true; // wake input consumed
}

void DisplayPowerManager::wake(const char *reason) {
  if (!begun_) {
    return;
  }
  last_activity_ms_ = millis();
  if (state_ != DisplayPowerState::Active) {
    enterActive(reason != nullptr ? reason : "wake");
  }
}

void DisplayPowerManager::sleep() {
  if (!begun_) {
    return;
  }
  enterOff(millis());
}

void DisplayPowerManager::enterActive(const char *reason) {
  const DisplayPowerState from = state_;
  if (from == DisplayPowerState::Off && panel_wake_ != nullptr) {
    panel_wake_();
  }
  state_ = DisplayPowerState::Active;
  logTransition(from, state_, reason);
  if (restore_ui_ != nullptr) {
    restore_ui_();
  }
}

void DisplayPowerManager::enterScreensaver(uint32_t now_ms) {
  const DisplayPowerState from = state_;
  state_ = DisplayPowerState::Screensaver;
  logTransition(from, state_, nullptr);
  ss_slot_ = 0;
  last_ss_move_ms_ = now_ms;
  if (draw_ss_ != nullptr) {
    draw_ss_(ss_slot_);
  }
}

void DisplayPowerManager::enterOff(uint32_t now_ms) {
  (void)now_ms;
  const DisplayPowerState from = state_;
  state_ = DisplayPowerState::Off;
  logTransition(from, state_, nullptr);
  if (panel_sleep_ != nullptr) {
    panel_sleep_();
  }
}

void DisplayPowerManager::update(uint32_t now_ms) {
  if (!begun_) {
    return;
  }

  const uint32_t idle = now_ms - last_activity_ms_;
  const uint32_t ss_ms = display_power_screensaver_ms(settings_);
  const uint32_t off_ms = display_power_screen_off_ms(settings_);

  if (state_ == DisplayPowerState::Active) {
    if (ss_ms > 0 && idle >= ss_ms) {
      enterScreensaver(now_ms);
    } else if (ss_ms == 0 && off_ms > 0 && idle >= off_ms) {
      // Screensaver disabled: jump straight to panel off.
      enterOff(now_ms);
    }
    return;
  }

  if (state_ == DisplayPowerState::Screensaver) {
    if (off_ms > 0 && idle >= off_ms) {
      enterOff(now_ms);
      return;
    }
    const uint32_t move_ms =
        settings_.screensaver_move_ms == 0 ? 8000 : settings_.screensaver_move_ms;
    if ((now_ms - last_ss_move_ms_) >= move_ms) {
      last_ss_move_ms_ = now_ms;
      ss_slot_ = static_cast<uint8_t>((ss_slot_ + 1) % 8);
      if (draw_ss_ != nullptr) {
        draw_ss_(ss_slot_);
      }
    }
    return;
  }

  // Off: wait for notifyActivity / wake. Touch polling stays outside.
}
