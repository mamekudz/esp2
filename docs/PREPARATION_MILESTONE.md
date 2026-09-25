# ESP][ preparation milestone (host-side)

Status: **IN PROGRESS** — architecture and host tests while additional BLE
input hardware is still in transit.

## Non-goals

- Do **not** implement the full Apple II emulator on-device in this pass.
- Do **not** rewrite verified DISPLAY / TOUCH / SD / IMU drivers.
- Do **not** flash speculative integration firmware that risks the baseline.
- Do **not** embed copyrighted Apple ROMs or commercial disk images.

## Git checkpoint

Hardware baseline commit (before preparation):

    af155ef  checkpoint: Phase-1 hardware baseline (DISPLAY/TOUCH/SD/IMU)

Verified conceptual regression (physical):

    DISPLAY=OK TOUCH=OK SD=OK IMU=OK

BLE scanner verified; VR PARK connection/HID **not** verified.

## Layout added by this milestone

    docs/architecture/     module boundaries, budgets, tasks, audio, ROM
    docs/apple2/           core evaluation, artifact color, NIB/WOZ, speaker
    docs/compatibility/    controller DB + references
    docs/diagnostics/      session model + IMU calibration plan
    docs/media/            game.json schema + disk format notes
    host/                  Node host tests (no ESP flash required)
    host/include/          C++ interface headers (platform-neutral)
    fixtures/synthetic/    demo library metadata + generated disk fixtures

## How to run host tests

From repo root (Node 22+):

    node host/test/run.mjs

Uses PlatformIO Python only if a future C++ harness is added. Current tests
are pure JavaScript for zero toolchain risk on Windows.

## Firmware policy

`src/` and known-good bring-up code stay frozen except for explicit,
incremental hardware milestones. Host modules must not `#include` Arduino
or ESP-IDF headers.
