# Architecture overview

ESP][ keeps concerns separated as required by `CLAUDE.md` §31.

```
                    +------------------+
                    |   ui / control   |
                    +--------+---------+
                             |
     +-----------+-----------+-----------+------------+
     |           |           |           |            |
 bluetooth    input       media      diagnostics   display
     |           |           |           |            |
     +-----+-----+           |           |            |
           |                 |           |            |
           v                 v           |            v
      +----+----+      +-----+-----+     |     +------+------+
      | apple2  |<---->|  video    |-----+---->| viewport /  |
      | machine |      | pipeline  |           | effects     |
      +----+----+      +-----------+           +------+------+
           |                                          |
           v                                          v
        audio                                    physical
                                                 AMOLED
                                                 (wrapper only)
```

## Module roots (target)

| Directory | Owns | Must not know |
| --- | --- | --- |
| `apple2/` | CPU, memory map, soft switches, speaker toggle, Disk II logic API | GPIO, NimBLE, CO5300, FAT paths |
| `video/` | decode, color modes, effects (logical) | board pins |
| `input/` | normalized keyboard/gamepad/paddle/touch/motion | BLE/USB transport details |
| `bluetooth/` | discovery, HID clients, BlueShift-as-generic-HID | Apple II soft switches |
| `media/` | VirtualDrive, DiskImage, StorageBackend | Apple II CPU internals |
| `audio/` | speaker events → PCM / local / future BLE audio | display |
| `diagnostics/` | session model, OK/NOT OK workflow | game logic |
| `ui/` | Control Screen, virtual keyboard presentation | disk bit timing |
| `hardware/` | known-good board bring-up wrappers | Apple II |

## Rules

1. Wrap verified Phase-1 drivers; do not replace them for cleanliness.
2. BlueShift is a **separate** product. ESP][ talks to it as ordinary BLE HID.
3. User-facing strings go through i18x; machine logs stay English/neutral tags.
4. Host tests exercise `apple2/`, `video/`, `media/`, `input/` without flashing.
