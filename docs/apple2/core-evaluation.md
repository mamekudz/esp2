# Apple II core evaluation (COMPONENT reuse)

Status: **fake6502 INCORPORATED** (host harness); firmware not linked yet.

Goal: choose components suitable for ESP32-S3 (RAM/PSRAM, no desktop deps),
not blindly port a full desktop emulator (`CLAUDE.md` §33).

## Selected core — INCORPORATED

| Field | Value |
| --- | --- |
| Core | **fake6502** v1.3 |
| Upstream | https://github.com/C-Chads/MyLittle6502 |
| Commit | `3078e2c337f78af68bcc675a2f27a9bd82e202da` |
| Path | `third_party/fake6502/fake6502.h` |
| License | Public domain / CC0 |
| Wrapper | `esp_bracket::Cpu6502` |
| Why | Permissive, tiny, portable C, memory callbacks, IRQ/NMI, instruction cycle table + page-cross/branch penalties, no desktop deps, BCD available (do not define `NES_CPU`) |
| Limits | Instruction-level (not cycle-stepping); not Apple II timing-exact; undocumented opcodes optional |

### Cycle behavior (documented)

| Aspect | Behavior |
| --- | --- |
| Accounting | Per-instruction base ticks (`ticktable`) |
| Page-cross | +1 when penalty flags set (e.g. abs,X / (zp),Y) |
| Branches | Taken/page-cross penalties via core |
| Interrupts | Handler entry via `irq6502` / `nmi6502` |
| Granularity | **Instruction-level**, not cycle-level bus stepping |

Do **not** claim cycle-exact Disk II / speaker / video until a finer model exists.

### Integration strategy

1. Vendor pinned header under `third_party/fake6502/` (compile as **C** — `and` is a C++ keyword).
2. Access only through `Cpu6502`.
3. Host tests via LLVM-MinGW under `tools/host-toolchain/` (gitignored).
4. Firmware link deferred to a later milestone.

Prior unfinished `vendor/fake6502/fake6502.c` was an identical untracked copy; replaced by the pinned tree.

## Comparison matrix

| Candidate | License | 6502 | Video/Disk | Portability to ESP32 | Notes |
| --- | --- | --- | --- | --- | --- |
| **fake6502** (MyLittle6502) | PD/CC0 | Yes | No | Excellent | **SELECTED / INCORPORATED** |
| lib6502 / educational cores | Often MIT/BSD | Yes | No | Excellent | EVALUATED |
| AppleWin | GPL-2.0 | Yes | Strong | Poor as a whole | Reference only |
| audetto/AppleWin | GPL-2.0 | Same | Strong | Heavy | Reference only |
| MAME apple2 | GPL-2.0+ | Yes | Very accurate | Poor | Oracle only |
| POM2 | GPL-3.0 | Yes | Rich | Poor for MCU | Reference only |
| microM8 | Proprietary | — | — | N/A | Not reusable |
| Apple ][js | MIT (code) | JS | Web | N/A as C++ | Catalog/UX reference |

## Explicit non-recommendations

- Do not vendor all of AppleWin or MAME into firmware.
- Do not depend on microM8 binaries/source.
- Do not commit Apple ROMs.

## Host verification

```
node host/tools/build_and_test_apple2.mjs
```
