# Hardware baseline (do not regress)

Git tag/commit policy for physical bring-up:

- Checkpoint commit: `af155ef` — Phase-1 hardware baseline
- Keep DISPLAY / TOUCH / SD / IMU known-good paths in `src/` and
  `include/board_pins.h` / `qmi8658_min.*` stable unless a dedicated
  hardware milestone says otherwise.

Conceptual regression:

| Subsystem | Status |
| --- | --- |
| DISPLAY CO5300 280×456 | OK (visual) |
| TOUCH FT3168 | OK (physical) |
| microSD SPI | OK |
| IMU QMI8658 byte-wise | OK (orientation mapping pending) |
| BLE scanner | OK |
| VR PARK HID | **not verified** |
| Display power (screensaver / OFF) | see `docs/architecture/display-power.md` |

See `docs/PREPARATION_MILESTONE.md` for host-side work that must not require
flashing risky integration builds.
