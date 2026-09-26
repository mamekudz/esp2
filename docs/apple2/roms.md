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
| `synthetic` | ESP][ Disk II boot tests |
| user path | `--slot6-rom` / future host flag — validate/hash only |

## Optional real-ROM test

```bash
gulp apple2:rom-test --rom path/to.rom
# or
npm run apple2:rom-test -- --rom path/to.rom
```

Loads via reset vector (no hardcoded BASIC entry), runs a cycle budget, prints diagnostics + text dump.
