# Apple II speaker model

## Hardware behavior (summary)

Apple II audio is produced by toggling a soft-switch address that flips a
1-bit speaker transducer. Software controls pulse timing; there is no
dedicated PCM DAC in the base machine.

## Emulation events

```
struct SpeakerEvent {
  uint32_t cycle;   // machine cycle stamp
  uint8_t level;    // 0 or 1 after toggle
};
```

The audio engine converts event streams into:

- click / square approximations for local piezo, or
- band-limited PCM for digital sinks.

## Separation

`AppleIIMachine` emits events only. GPIO / I2S / BLE Audio live outside.
