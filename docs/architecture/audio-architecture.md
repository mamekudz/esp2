# Audio architecture (preparation only)

```
Apple II speaker soft-switch toggle
            |
      audio engine (host-testable)
            |
     +------+------+
     |             |
  local sink    Bluetooth sink (later)
  (piezo/DAC)   (ONLY if BLE Audio proven)
```

## Constraints

- ESP32-S3 is **BLE-only** (no Classic / A2DP assumption).
- BLE Audio feasibility must be measured experimentally; do not implement yet.
- Do not couple speaker emulation to a GPIO in the Apple II core.

## Engine responsibilities

- Convert toggle timestamps / cycle counts into a PCM or click stream.
- Handle underruns with diagnostics.
- Volume / mute as UI state, not core state.

See also: `docs/apple2/speaker-model.md`.
