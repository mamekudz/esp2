# User-supplied Apple II / II+ ROMs

ESP][ **does not ship Apple ROMs**. The repository stays ROM-free.

## Policy

- Do not download, scrape, commit, or embed proprietary Apple ROM bytes.
- Users supply ROMs they have the right to use.
- CI and `npm run test:apple2` stay green **without** proprietary ROMs.

## Local layout

```
local/roms/           # gitignored except README
  apple2plus.rom      # example name — your 12 KiB motherboard ROM

config/roms.local.example.json
config/roms.local.json   # gitignored — your paths
```

Expected motherboard size: **12288 bytes** (`$D000–$FFFF`).

## Identification

```bash
node host/tools/rom_test.mjs --identify-only --rom path/to.rom
# or
gulp apple2:rom-identify --rom path/to.rom
```

Reports SHA-256, size, and metadata match status:

| Status | Meaning |
| --- | --- |
| OK | Hash listed in `host/data/rom_database.json` |
| UNKNOWN_ROM | Valid size, hash not in DB (still loadable) |
| INVALID_SIZE | Rejected |
| SKIPPED_NO_ROM | No local ROM — not a CI failure |

The metadata DB contains **hashes only**, never ROM bytes. Entries require reliable provenance; uncertain hashes are omitted.

## Machine profiles

| Profile | Notes |
| --- | --- |
| AppleII | Integer / original II class |
| AppleIIPlus | Autostart + Applesoft class (default host assumption when unknown) |

Apple IIe is **not** implemented.

## Slot-6 ROM

Independent of motherboard ROM:

| Mode | Use |
| --- | --- |
| `none` | Default for real-ROM / BASIC bring-up |
| `synthetic` | Level-2 Disk II softswitch / marker tests |
| `cleanroom` | Level-4 realistic boot (project-owned) |
| user path | `--slot6-rom <path>` or `--slot6 <path>` — 256-byte PROM, SHA-256 identity |

Metadata: `host/data/slot6_rom_database.json` (hashes only). Missing local PROM →
`SKIPPED_NO_SLOT6_ROM` (not a CI failure).

```bash
host/.out/esp2_host.exe --slot6-rom path/to/diskii.rom --disk1 Esp2BootTest --batch
```

## Optional real-ROM test

```bash
gulp apple2:rom-test --rom path/to.rom
# or
npm run apple2:rom-test -- --rom path/to.rom
```

Loads via reset vector (no hardcoded BASIC entry), runs a cycle budget, prints diagnostics + text dump.
