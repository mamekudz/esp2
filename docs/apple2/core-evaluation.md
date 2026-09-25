# Apple II core evaluation (COMPONENT reuse)

Status: **EVALUATED only** — no third-party emulator source incorporated yet.

Goal: choose components suitable for ESP32-S3 (RAM/PSRAM, no desktop deps),
not blindly port a full desktop emulator (`CLAUDE.md` §33).

## Comparison matrix

| Candidate | License | 6502 | Video/Disk | Portability to ESP32 | Notes |
| --- | --- | --- | --- | --- | --- |
| **fake6502** (various public-domain ports) | Public domain / CC0 (verify fork) | Yes | No | Excellent | Tiny CPU core; good host-test start |
| **lib6502** / similar educational cores | Often MIT/BSD (verify) | Yes | No | Excellent | Prefer well-tested + cycle hooks |
| **AppleWin** | GPL-2.0 | Yes (integrated) | Strong (NTSC, WOZ, cards) | Poor as a whole | Desktop/Win32 history; huge; license copyleft |
| **audetto/AppleWin** | GPL-2.0 (fork) | Same lineage | Strong | Better cross-platform, still heavy | Useful as **reference behavior**, not drop-in |
| **LinApple / linappleii** | GPL (lineage) | Yes | SDL UI heavy | Medium/Poor | Aging forks; SDL dependency |
| **MAME apple2 driver** | GPL-2.0+ | Yes | Very accurate | Poor | Too large; good oracle for tests |
| **POM2** | GPL-3.0 | Yes | Rich | Poor for MCU | Browser/desktop oriented |
| **microM8** | Proprietary / closed | — | — | N/A | Not reusable as source |
| **Apple ][js** (Scullin) | Check upstream | JS | Web | N/A as C++ | UX reference for library UX only |

## Evaluation criteria (applied)

| Criterion | Prefer |
| --- | --- |
| License | Permissive for isolated CPU; document GPL if any component pulled |
| Dynamic allocation | Minimal / none in hot path |
| Filesystem assumptions | Abstract behind StorageBackend |
| Endian | Little-endian ESP32-S3 OK; avoid BE assumptions |
| Timing model | Cycle accounting desirable for Disk II later |
| Testability | Host unit tests without ROM blobs |
| Code size | Keep under tight flash budget |

## Recommendation

### Phase A (now / host)

1. Adopt or reimplement a **small 6502 core** under a clear permissive
   license (candidate class: fake6502-style). Run host tests first.
2. Use AppleWin / MAME / published NTSC notes as **behavioral references**
   for artifact color — do not copy large GPL trees yet.
3. Write Apple II memory map / soft switches in-project (clean-room), tested
   on host.

### Phase B (later, on-device)

1. Integrate only the CPU + our memory/soft-switch layer.
2. Add Disk II incrementally (DSK/PO first; NIB/WOZ after research).
3. If a GPL video/disk component becomes necessary, decide project license
   impact **before** copying files.

## Explicit non-recommendations

- Do not vendor all of AppleWin or MAME into firmware.
- Do not depend on microM8 binaries/source.
- Do not commit Apple ROMs.

## Next concrete step

Host 6502 harness skeleton + synthetic reset-vector ROM stub (no Apple IP).
