# Display power management (AMOLED)

Status: **IMPLEMENTED** on Phase-1 diagnostic firmware (panel only).

## Purpose

Reduce static-image AMOLED stress and unnecessary panel on-time during
long diagnostic sessions. ESP32 itself is **not** put to sleep.

## States

| State | Meaning |
| --- | --- |
| `ACTIVE` | Normal UI; panel on |
| `SCREENSAVER` | Mostly black + relocating `ESP][` mark |
| `OFF` | CO5300 `displayOff()` (`DISPOFF` + `SLPIN`) |

Transitions:

    ACTIVE --inactivity--> SCREENSAVER --further inactivity--> OFF
    SCREENSAVER|OFF --activity--> ACTIVE

## Defaults (production)

| Setting | Preset | Effective |
| --- | --- | --- |
| Screensaver | `Min2` | 2 minutes |
| Screen off | `Min5` | 5 minutes |
| Screensaver relocate | — | 8 seconds |

Future Control Screen presets (enums already defined):

- Screensaver: Off / 1 / 2 / 5 min
- Screen off: Never / 2 / 5 / 10 min

## Wake / first-touch consume

Any activity (today: touch) resets the idle clock.

If the display was in `SCREENSAVER` or `OFF`, `notifyActivity()` returns
`true` and the firmware **must not** forward that input to UI hit-tests
(e.g. BLE “SCAN AGAIN”). The next touch after wake operates the UI.

Future BLE keyboard / gamepad / physical controls should call the same API
with a language-neutral reason (`keyboard`, `gamepad`, …).

## Serial diagnostics

Transition-only logs:

    [DISPLAY-POWER] ACTIVE -> SCREENSAVER
    [DISPLAY-POWER] SCREENSAVER -> OFF
    [DISPLAY-POWER] OFF -> ACTIVE reason=touch

## Lab short-timeout build

Add `-DDISPLAY_POWER_TEST_SHORT` to `build_flags` for physical verify:

- screensaver = 10 s
- off = 20 s
- relocate = 3 s

Remove the define for production defaults.

### Short-test serial results (2026-09-25)

Verified on Waveshare AMOLED board (COM5):

- `[REGRESSION] DISPLAY=OK TOUCH=OK SD=OK IMU=OK`
- `ACTIVE -> SCREENSAVER` then `SCREENSAVER -> OFF`
- IMU samples continued while panel OFF
- Touch heartbeat polls continued while panel OFF (`TOUCHHB poll ok` increasing)
- No watchdog/reset observed during OFF window

Wake-on-touch / first-touch consume: implemented in firmware; exercise with a
physical tap after OFF (serial expects `OFF -> ACTIVE reason=touch`).

Note: long blocking BLE scans delay `update()` until the loop resumes; idle
is still wall-clock based once `update()` runs.

## Code

- `include/display_power.h`
- `src/display_power.cpp`
- wired from `src/main.cpp` (does not rewrite CO5300 init)
