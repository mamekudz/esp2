# User-supplied Apple II / II+ ROMs

ESP][ **does not ship Apple ROMs**. The repository stays ROM-free.

## Policy

- Do not download, scrape, commit, or embed proprietary Apple ROM bytes.
- Users supply ROMs they have the right to use.
- CI and `npm run test:apple2` stay green **without** proprietary ROMs.

## Local layout (host)

```
local/roms/           # gitignored except README — user-supplied proprietary ROMs
  apple2plus.rom

local/apple2/         # gitignored except README — acquisition workflow root
  roms/appleiigo.rom  # public-domain replacement via gulp apple2:rom:sync
  disks/              # prepared runtime images
  cache/              # downloaded apple2js sources (LOCAL_TEST_ONLY)

config/roms.local.example.json
config/roms.local.json   # gitignored — your paths
```

See `docs/media/local-apple2-workflow.md` for the µGulp acquisition tasks.
Do **not** commit ROM/disk payloads under `local/apple2/`.

## microSD layout (ESP32 PART F1)

```
/esp2/roms/
  system.rom          # preferred deterministic name (12288 bytes)
  apple2plus.rom      # alternate
  apple2.rom          # alternate
  profile.txt         # optional: AppleII or AppleIIPlus when hash unknown
```

Any other 12288-byte file under `/esp2/roms/` is also accepted (scanned by size,
not filename alone). ROM files on the SD card are runtime assets and must never
enter Git.

Expected motherboard size: **12288 bytes** (`$D000–$FFFF`).

## Identification

```bash
node host/tools/rom_test.mjs --identify-only --rom path/to.rom
# or
gulp apple2:rom-identify --rom path/to.rom
```

On ESP32 load, firmware reports:

```
[ROM] path=...
[ROM] size=12288
[ROM] sha256=...
[ROM] class=KNOWN_APPLE_II|KNOWN_APPLE_II_PLUS|UNKNOWN_SUPPORTED_SIZE|UNSUPPORTED
[ROM] profile=AppleII|AppleIIPlus
```

| Status | Meaning |
| --- | --- |
| OK / known hash | Hash listed in `host/data/rom_database.json` |
| UNKNOWN_SUPPORTED_SIZE | Valid 12 KiB, hash not in DB (still loadable) |
| UNSUPPORTED | Rejected size |
| SKIPPED_NO_ROM | No local/SD ROM — not a CI failure |

The metadata DB contains **hashes only**, never ROM bytes. Entries require reliable provenance; uncertain hashes are omitted.

## Machine profiles

| Profile | Notes |
| --- | --- |
| AppleII | Integer / original II class |
| AppleIIPlus | Autostart + Applesoft class |

Apple IIe is **not** implemented. If identification is uncertain, set
`/esp2/roms/profile.txt` explicitly rather than guessing IIe behavior.

## Mapping (host == ESP32)

Same portable `Rom` / `Apple2Bus` mapping:

| Region | Notes |
| --- | --- |
| `$D000–$FFFF` | 12 KiB motherboard ROM |
| `$FFFC/$FFFD` | RESET vector (CPU must take PC from here) |
| `$FFFE/$FFFF` | IRQ/BRK |
| `$FFFA/$FFFB` | NMI |
| Writes to ROM window | Ignored (read-only) |

No ESP32-specific ROM map. No PC shortcuts into BASIC/Monitor.

## Slot-6 ROM

Independent of motherboard ROM. **PART F1 default: NONE** (observe genuine
system-ROM startup; do not auto-boot Esp2BootTest).

| Mode | Use |
| --- | --- |
| `none` | Default for real-ROM / BASIC bring-up (F1) |
| `synthetic` | Level-2 Disk II softswitch / marker tests |
| `cleanroom` | Level-4 realistic boot (project-owned; F1 spot regression only) |
| user path | `--slot6-rom <path>` or `--slot6 <path>` — 256-byte PROM, SHA-256 identity |

Metadata: `host/data/slot6_rom_database.json` (hashes only). Missing local PROM →
`SKIPPED_NO_SLOT6_ROM` (not a CI failure).

```bash
host/.out/esp2_host.exe --slot6-rom path/to/diskii.rom --disk1 Esp2BootTest --batch
```

## Optional real-ROM test (host)

```bash
gulp apple2:rom-test --rom path/to.rom
# or
npm run apple2:rom-test -- --rom path/to.rom
```

Loads via reset vector (no hardcoded BASIC entry), runs a cycle budget, prints diagnostics + text dump.

## PART F1 physical status

| Item | Status |
| --- | --- |
| Firmware path (SD → hash → map → RESET → keyboard `$C000/$C010`) | Implemented (`apple2_text` / `apple2_rom_f1b`) |
| Level-4 clean-room spot (Slot-6 temporary) | **PASS** (stack-safe `a2boot` task) |
| User ROM on microSD | **SKIPPED_NO_ROM** until `/esp2/roms/system.rom` is supplied |
| `REAL_SYSTEM_ROM` | **NOT_VERIFIED** (blocked on user asset) |
| `LEVEL_5` | **BLOCKED_NO_USER_ROM** (harness remains READY) |

Do **not** document or commit proprietary ROM contents or hashes from private dumps
unless the project metadata policy explicitly records them.
