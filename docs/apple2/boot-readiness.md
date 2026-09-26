# Boot readiness (host + ESP32)

Machine-readable companion: `boot-readiness.json`

## Levels

| Level | Meaning | Current |
| --- | --- | --- |
| 0 | Synthetic CPU tests | met |
| 1 | Synthetic ESP][ test ROM | met |
| 2 | Disk II synthetic boot (no Apple ROM/DOS) | met |
| 3 | User-supplied II/II+ ROM + interactive text path | met (host) |
| 4 | Realistic 6502 Slot-6 Disk II boot (clean-room) | **HOST_VERIFIED** + **ESP32_PHYSICALLY_VERIFIED** |
| 5 | Real user-supplied software, interactive, no title hacks | **READY_FOR_REAL_SOFTWARE_TEST** (not PASS); F1 ROM path **BLOCKED_NO_USER_ROM** on device |

Level 5 is **not** PASS until a legal local ROM + media configuration boots
interactively. PART F1 does **not** promote full Level 5 PASS.

## Subsystem gate

| Subsystem | Status |
| --- | --- |
| CPU | PASS |
| RAM | PASS |
| ROM abstraction + loader/hash | PASS |
| keyboard / strobe | PASS |
| speaker | PASS |
| video switches / text / LoRes / HGR / artifact | PASS (host + ESP32 video) |
| Slot-6 modes + Level-4 clean-room boot | PASS host; **ESP32 DSK physical PASS** |
| Compatibility harness | PASS (no proprietary media required) |
| Language Card | DEFERRED |
| WOZ / writes | DEFERRED |
| ESP32 Disk II + microSD | **PASS** (PART E) |
| ESP32 user system ROM (PART F1) | Firmware path ready; **SKIPPED_NO_ROM** until SD asset |

## ESP32 Level-4 evidence (project-owned)

Path: `/esp2/disks/Esp2BootTest.dsk` → `Esp32SdStorageBackend` → PSRAM
`Dos33NibbleImage` → `DiskIIController` → `$C600` clean-room PROM → `$C0EC`
nibble path → denibble `$C800` → markers `$03FE=$4C` `$03FF=$34` + text via
Apple II video RAM → CO5300.

Rotational start variants (0, 37, 128, 777) required — boot must not depend on
sector alignment.

## ESP32 PART F1 (user system ROM)

Intended path:

```
/esp2/roms/system.rom
  → size + SHA-256 + RomDatabase classify
  → map $D000–$FFFF (same as host)
  → CPU RESET (PC from $FFFC/$FFFD only)
  → Slot 6 NONE
  → interactive text detect
  → keyDown → $C000/$C010
  → PRINT 2+2 (Applesoft / II+) when applicable
  → CO5300 via shared video pipeline
```

Physical evidence (`apple2_rom_f1b`): Level-4 spot **PASS**, DISPLAY/TOUCH/SD/IMU
**PASS**, live cps ≈ 1.023e6, stability **PASS**. No user ROM on microSD →
`REAL_SYSTEM_ROM=NOT_VERIFIED`. Place a legal 12288-byte ROM at
`/esp2/roms/system.rom` and reflash/reboot to complete F1 criteria A–J.
