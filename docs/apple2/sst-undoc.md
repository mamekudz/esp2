# SST undoc oracle (HOST)

Independent validation of selected NMOS undocumented opcodes using
[SingleStepTests/65x02](https://github.com/SingleStepTests/65x02).

## Policy

| Item | Policy |
| --- | --- |
| Vector JSON (`*.json`) | **gitignored** under `local/apple2/forensics/sst/` |
| Packed `.sstb` | **gitignored** (generated) |
| Tooling source | **tracked** (`host/tools/sst_pack.mjs`, `run_sst_undoc.mjs`, `host/test_cpp/sst_undoc_runner.cpp`) |
| CI | Green **without** local vectors |

Do **not** commit multi-megabyte vector payloads.

## Format

JSON **array** of cases (65x02 `6502/v1/<op>.json`):

```text
{
  "name": "…",
  "initial": { "pc", "s", "a", "x", "y", "p", "ram": [[addr,val],…] },
  "final":   { "pc", "s", "a", "x", "y", "p", "ram": [[addr,val],…] },
  "cycles":  [[addr, data, "read"|"write"], …]
}
```

Integers are decimal. Runner checks registers + `final.ram` after one
`Cpu6502::step()`.

## How to run

```text
# Place vectors (example):
#   local/apple2/forensics/sst/{9c,9e,9f,cb}.json

node host/tools/build_and_test_apple2.mjs   # builds sst_undoc_runner.exe
node host/tools/run_sst_undoc.mjs
```

Or manually:

```text
node host/tools/sst_pack.mjs --all
host/.out/sst_undoc_runner.exe local/apple2/forensics/sst/9c.sstb
```

## Verified result (this milestone)

| Opcode | Available | Executed | PASS | FAIL |
| --- | --- | --- | --- | --- |
| `$9C` | 10000 | 10000 | 10000 | 0 |
| `$9E` | 10000 | 10000 | 10000 | 0 |
| `$9F` | 10000 | 10000 | 10000 | 0 |
| `$CB` | 10000 | 10000 | 10000 | 0 |

`EXTERNAL_APPLEWIN_ORACLE = NOT_AVAILABLE / NOT_REQUIRED_YET`
