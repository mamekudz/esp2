# Fake6502 (INCORPORATED)

| Field | Value |
| --- | --- |
| Upstream | https://github.com/C-Chads/MyLittle6502 |
| File | `fake6502.h` (header-only MOS 6502 core) |
| Pinned commit | `3078e2c337f78af68bcc675a2f27a9bd82e202da` |
| Version | v1.3 (Mike Chambers + David MHS Webster / gek169) |
| License | Public domain / CC0 (header states FULLY PUBLIC DOMAIN, CC0) |
| SHA-256 | `CC74213E5457AE0A8620B6AA996E2976A4C0AFE049BE625FDB367AC1135CED25` |

## ESP][ integration notes

- Do **not** enable `NES_CPU` (Apple II requires BCD for ADC/SBC).
- `UNDOCUMENTED` may remain defined; Apple II software often needs it.
- ESP][ patch: opcode `$CB` is NMOS **SBX/AXS** (upstream left it as NOP).
  Required for some Disk II / DOS boot obfuscation paths.
- ESP][ patch: `$9C` **SHY** abs,X, `$9E` **SHX** abs,Y, `$93`/`$9F` **AHX/SHA**
  (upstream NOP). Mask uses base high-byte+1; on indexed page-cross the store
  address high byte is rewritten to the stored value (NMOS unstable behavior,
  matched to SingleStepTests/65x02).
- ESP][ patch: RMW undocumented composites **ISB/DCP/SLO/RLA/SRE/RRA** feed the
  ALU-modified byte into the secondary op via a one-shot `getvalue` override.
  Upstream `inc(); sbc();` re-reads memory after `putvalue`; on ROM /
  write-ignored targets that wrongly SBCs the original byte (Apple II titles
  such as Galaxian use `ISB abs,Y` against `$Dxxx`).
- Access only through `esp_bracket::Cpu6502` — never call `step6502` from app code.
- Do not clang-format this file.
- Cycle model: **instruction-level** with base tick table + page-cross /
  branch penalties. Not a cycle-stepping core.

## Prior unfinished copy

`vendor/fake6502/fake6502.c` was an untracked identical copy of this header
(same SHA-256). Replaced by this pinned `third_party/` incorporation.
