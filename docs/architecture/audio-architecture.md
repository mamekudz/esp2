# Audio architecture

```
Apple II $C030 toggle (any access)
            |
     Speaker (1-bit + cycle-stamped edges)
            |
     SpeakerPcmRenderer (interval integration)
            |
     HostAudioBackend / future ESP32 / BlueShift reconstruct
```

Authoritative fidelity rules: `docs/apple2/audio.md` and CLAUDE.md §17.

## BlueShift™ (document only — not in this repository)

Preferred wire format: **cycle-delta edges**, not PCM.

Haptic / other vendor commands: same optional extension channel later;
**NOT_IMPLEMENTED** now.
