# Undocumented NMOS 6502 opcodes (ESP][ host)

Status of ESP][ patches on vendored fake6502. Distinguishes:

| Level | Meaning |
| --- | --- |
| implemented | Opcode handler present (not NOP) |
| unit-tested | Compact tracked regression in `test_cpu_main.cpp` |
| SST-validated | Full SingleStepTests/65x02 vector set PASS (local vectors) |

Only the opcodes below are claimed SST-validated. The rest of the NMOS
undocumented matrix is **not** asserted complete.

## SST-validated (10000 / 10000 each)

| Opcode | Mnemonic | implemented | unit-tested | SST-validated |
| --- | --- | --- | --- | --- |
| `$9C` | SHY abs,X | yes | yes (incl. page-cross) | **yes** |
| `$9E` | SHX abs,Y | yes | yes (via SHY/AHX suite) | **yes** |
| `$9F` | AHX/SHA abs,Y | yes | yes | **yes** |
| `$CB` | SBX/AXS #imm | yes | yes | **yes** |

Vector source, local path, and runner: `docs/apple2/sst-undoc.md`.

High-byte / page-cross behavior matches NMOS unstable stores (not 65C02).

## Other implemented undocs (not SST-validated here)

| Opcode family | Notes |
| --- | --- |
| LAX / SAX | Used by Galaxian Stage A/B |
| ISB/DCP/SLO/RLA/SRE/RRA | RMW composites; ESP][ patch feeds ALU result after write-ignored ROM |
| `$93` AHX indy | Shares `ahx()` handler; no dedicated SST run in this milestone |

## Galaxian execution before divergence

| Opcode | Executed? |
| --- | --- |
| `$9C` | yes |
| `$9E` | yes |
| `$9F` | yes |
| `$CB` | no |

Current Galaxian blocker is **not** these opcodes — see
`docs/apple2/boot-forensics.md` (Apple II+ ROM **data** dependency).
