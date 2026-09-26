# Apple II speaker model (cycle-accurate 1-bit)

See **`docs/apple2/audio.md`** for the full fidelity specification,
Star Blazer subjective reference, BlueShift notes, and diagnostics.

## Emulation events

```
struct SpeakerEvent {
  uint32_t cycle;   // machine cycle stamp
  uint8_t level;    // 0 or 1 after toggle (1-bit state)
};
```

Optional transport packing uses `deltaCycles` between edges without
discarding timing.
