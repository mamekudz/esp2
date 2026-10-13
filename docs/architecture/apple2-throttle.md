# Apple II real-time throttle (ESP32)

Labels: **PHYSICALLY_VERIFIED** · **HOST_VERIFIED** · **DOCUMENTED**

Wall-clock throttling keeps emulated Apple II speed near **1.023 MHz**. Emulated
**cycles** remain the authoritative Apple II timeline (speaker, paddles, Disk II).
FreeRTOS / wall-clock time is only for real-time pacing and UI.

## Permanent rule

Long-running Apple II throttle timing must use a **monotonic timebase that does
not wrap** on normal operating timescales.

## Long-run absolute timebase (PHYSICALLY_VERIFIED)

| Role | Implementation |
| --- | --- |
| API | `esp_timer_get_time()` |
| Type | `int64_t` microseconds |
| Anchor | `g_wallStartUs` at emu wall-clock sync |

Do **not** use Arduino `micros()` as this absolute throttle timebase.

### Former regression (closed)

| Item | Detail |
| --- | --- |
| Old timebase | `micros()` (`uint32_t` µs) |
| Wrap | ≈ **71 minutes 35 seconds** (~2³² µs) |
| Failure mode | After wrap, throttle could compute an enormous sleep and stop 6502 execution while ESP32 / display / RTOS stayed alive |
| Evidence | Reset-free freeze: PC stuck (e.g. `$94BC`), **cycles unchanged** |

This was an ESP][ throttle bug — **not** a Galaxian, opcode, Disk II, or display bug.

### Defensive sleep guard

If computed `sleepUs` exceeds **2 000 000** µs (~2 s), the firmware calls
`resyncEmuWallClock()` instead of sleeping forever. Normal delays are sliced to
**≤ 50 ms** FreeRTOS delays (or short `delayMicroseconds` for small gaps).

## Short-duration `micros()` (still legitimate)

`micros()` may still be used for **short** interval measurements such as:

- render timing (`g_lastRenderUs`)
- transfer timing (`g_lastXferUs`)

Those intervals are far below the 32-bit wrap horizon. The closed defect is only
its use as the **long-running absolute throttle** clock.

## Regression evidence

| Evidence | Kind | Location / result |
| --- | --- | --- |
| Simulated 32-bit wrap boundary | **HOST_VERIFIED** | `dev/tools/throttle-wrap.test.mjs` |
| Galaxian ≥ 2 h continuous run past old wrap | **PHYSICALLY_VERIFIED** | `docs/apple2/galaxian-2h-soak.json` |

Host wrap simulation does **not** replace the physical 2-hour soak; the physical
soak does **not** replace the fast host unit test.

### Claims (do not over-extend)

| ID | Claim | Status |
| --- | --- | --- |
| `MICROS_WRAP_REGRESSION` | ~71.6 min `micros()`-wrap throttle stall no longer reproduces on physical firmware | **PASS** |
| `GALAXIAN_2H_SOAK` | Galaxian completed a ≥ 2 h physical soak with continuing cycles / ~1.023 MHz cps | **PASS** |

These do **not** mean ESP][ or all Apple II software is 24 h–stable. Longer
(8 h / 12 h / 24 h) soaks remain optional **general** stability testing, not
required to re-validate this specific wrap fix.

### Display / frame counters

A flat `frames=` counter between snapshots is **not** definitive display-liveness
evidence. Prefer advancing **cycles**, changing **PC**, stable **cps**, and
visual confirmation when judging long-run health.

## Code authority

Firmware: `src/apple2_text/main.cpp` (`resyncEmuWallClock`, emu task throttle loop).
