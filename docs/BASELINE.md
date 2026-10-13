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
| Apple II long-run throttle (`esp_timer` / int64) | **OK** — `MICROS_WRAP_REGRESSION = PASS` |
| Galaxian ≥ 2 h physical soak | **OK** — `GALAXIAN_2H_SOAK = PASS` (not a 24 h claim) |

See `docs/architecture/apple2-throttle.md`, `docs/apple2/galaxian-2h-soak.json`.

See `docs/PREPARATION_MILESTONE.md` for host-side work that must not require
flashing risky integration builds.
