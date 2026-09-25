#pragma once

#include <Arduino.h>
#include <cstdint>

// AMOLED display power / anti-burn-in states.
// Does NOT put the ESP32 into deep sleep — panel only.
enum class DisplayPowerState : uint8_t {
  Active = 0,
  Screensaver,
  Off
};

// Future Control Screen presets (language-neutral enum ids).
enum class ScreensaverTimeout : uint8_t {
  Off = 0,
  Min1 = 1,
  Min2 = 2,
  Min5 = 5
};

enum class ScreenOffTimeout : uint8_t {
  Never = 0,
  Min2 = 2,
  Min5 = 5,
  Min10 = 10
};

struct DisplayPowerSettings {
  ScreensaverTimeout screensaver = ScreensaverTimeout::Min2;
  ScreenOffTimeout screen_off = ScreenOffTimeout::Min5;
  // Lab-only overrides (ms). Non-zero beats the preset. Production keeps 0.
  uint32_t screensaver_override_ms = 0;
  uint32_t screen_off_override_ms = 0;
  // How often the bouncing element relocates while in Screensaver.
  uint32_t screensaver_move_ms = 8000;
};

uint32_t display_power_screensaver_ms(const DisplayPowerSettings &s);
uint32_t display_power_screen_off_ms(const DisplayPowerSettings &s);

/**
 * Central inactivity → screensaver → panel-off manager.
 *
 * notifyActivity():
 *   - Always resets the inactivity clock.
 *   - If state was Screensaver or Off, wakes to Active and returns true
 *     so the caller can consume that input (wake touch must not hit UI).
 *   - If already Active, returns false (input proceeds normally).
 */
class DisplayPowerManager {
public:
  using DrawScreensaverFn = void (*)(uint8_t slot_index);
  using RestoreUiFn = void (*)();
  using PanelSleepFn = void (*)();
  using PanelWakeFn = void (*)();

  void begin(DrawScreensaverFn draw_ss, RestoreUiFn restore_ui,
             PanelSleepFn panel_sleep, PanelWakeFn panel_wake);

  void setSettings(const DisplayPowerSettings &settings);
  DisplayPowerSettings settings() const { return settings_; }

  bool notifyActivity(const char *reason);
  void update(uint32_t now_ms);

  void wake(const char *reason);
  void sleep(); // force Off (panel)

  DisplayPowerState state() const { return state_; }
  bool isInteractive() const { return state_ == DisplayPowerState::Active; }

private:
  void enterScreensaver(uint32_t now_ms);
  void enterOff(uint32_t now_ms);
  void enterActive(const char *reason);
  void logTransition(DisplayPowerState from, DisplayPowerState to,
                     const char *reason);
  static const char *stateName(DisplayPowerState s);

  DisplayPowerSettings settings_{};
  DisplayPowerState state_ = DisplayPowerState::Active;
  uint32_t last_activity_ms_ = 0;
  uint32_t last_ss_move_ms_ = 0;
  uint8_t ss_slot_ = 0;
  bool begun_ = false;

  DrawScreensaverFn draw_ss_ = nullptr;
  RestoreUiFn restore_ui_ = nullptr;
  PanelSleepFn panel_sleep_ = nullptr;
  PanelWakeFn panel_wake_ = nullptr;
};
