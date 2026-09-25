# Media / catalog source audit (ESP][)

Status: **EVALUATED** — host tooling milestone. No firmware changes.

ESP][ is not affiliated with any listed upstream.

## Provenance gate (enforced in code)

| Status | Auto-fetch media? | Metadata OK? |
| --- | --- | --- |
| REDISTRIBUTABLE | Only with stored evidence + explicit developer action | Yes |
| USER_SUPPLIED_ONLY | **Never** | Yes |
| UNKNOWN | **Never** | Yes |
| DO_NOT_DISTRIBUTE | **Never** | Caution |

Silent upgrades `USER_SUPPLIED_ONLY → REDISTRIBUTABLE` are **blocked** unless
review explicitly allows (`assertNoSilentRightsUpgrade`).

## Sources

### apple2js

- **Provides:** Website/git catalog metadata; JSON disk encoding; UX reference
- **License:** MIT for emulator software
- **Media:** Website blobs are **not** covered by MIT
- **ESP][:** Metadata sync/audit/import association only
- **Fetch media?** No automatic fetch

### a2kit (https://github.com/dfgordon/a2kit)

- **Provides:** Scriptable CLI/library — DOS 3.x, ProDOS, DSK/DO/PO/NIB/WOZ/2MG, …
- **License:** MIT
- **Usefulness:** Excellent optional **validation oracle** for import/parsers
- **ESP][:** Optional external `media:inspect` later; **not** a build dependency
- **Fetch media?** No

### DiskM8 (https://github.com/paleotronic/diskm8)

- **Provides:** CLI cataloging, DSK/PO/2MG/NIB, fingerprints, dupe detection
- **License:** **GPL-3.0**
- **ESP][:** External binary invoke only if needed — **do not incorporate GPL source**
- **Fetch media?** No

### EWM (https://github.com/st3fan/ewm)

- **Provides:** Apple 1 / ][+ emulator; Disk II; WOZ-oriented history
- **License:** MIT (Cargo.toml / headers)
- **ESP][:** Behavioral reference for Disk II / timing — no code copied yet
- **Fetch media?** No

### tcjennings/apple2

- **Claim:** README: MIT “applies to all contents of this repository”
- **LICENSE:** MIT (Toby Jennings, 2019)
- **Contents audited:** `DSK/screen_address_dos33.woz` (WOZ2, display-address demo)
- **ESP][ decision:** Treat that WOZ as **REDISTRIBUTABLE** with evidence;
  copied to `fixtures/redistributable/tcjennings-screen-address/` with NOTICE
- **Caution:** Re-verify if the repo later adds third-party dumps

### 4am / Internet Archive

- Collections such as `apple_ii_library_4am`, `softwarelibrary_apple`
- **Useful for:** Hash/catalog research
- **Not:** Bulk download into ESP][ or assumed redistribution rights
- Many titles remain commercial / mixed

### Total Replay (4cade)

- Launcher source: MIT (a2-4am/4cade)
- Distribution: large HDV on Archive.org (~160MB+ item)
- Contained games: still third-party — **USER_SUPPLIED_ONLY** for ESP][
- Hardware: 64K+ Apple II; some titles need 128K / joystick
- ESP][ V1 focuses on floppy-style titles first — Total Replay is future/optional

## Format support matrix (tooling)

| Format | IMPORT_RECOGNIZED | PARSER_SUPPORTED | EMULATOR_SUPPORTED |
| --- | --- | --- | --- |
| .dsk / .do | yes | yes (host) | planned |
| .po | yes | yes (host) | planned |
| .nib | yes | no | research |
| .woz | yes | no | research |
| .2mg | yes (header) | no | later |
| .hdv | yes (size check) | no | not V1 focus |

Recognition ≠ emulator readiness.
