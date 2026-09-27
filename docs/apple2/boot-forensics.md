# Apple II boot forensics (HOST)

Status: **HOST diagnostic tooling** for post-disk-load CPU/boot failures.

## Tool

```text
host/.out/boot_forensic.exe
  --rom local/apple2/roms/appleiigo.rom
  --disk local/apple2/disks/<title>.dsk
  --boot-disk
  --ram-init zero|ones|random
  --post-handoff 512
  --out-dir local/apple2/forensics
```

Writes only under gitignored `local/apple2/forensics/`:

| File | Contents |
| --- | --- |
| `handoff_meta.txt` | PC/A/X/Y/SP/P/cycle + patch bytes + ZP (no code dump) |
| `ram_b000_bfff.sha256` | Hash of loaded `$B000–$BFFF` (local only) |
| `trace_ring.txt` | Bounded post-handoff instruction ring |
| `opcodes.txt` | Opcode inventory for the window |

Committed regression fixture (metadata only):

`fixtures/golden/apple2/boot_handoff_galaxian_meta.txt`

## SST undoc oracle (HOST, local vectors)

Source: [SingleStepTests/65x02](https://github.com/SingleStepTests/65x02) `6502/v1/{9c,9e,9f,cb}.json`
(local only under `local/apple2/forensics/sst/` — **gitignored**, never commit).

### Format

JSON **array** of vectors (typically 10000 each). Each element:

```text
{
  "name": "9c 1",
  "initial": { "pc", "s", "a", "x", "y", "p", "ram": [[addr, val], ...] },
  "final":   { "pc", "s", "a", "x", "y", "p", "ram": [[addr, val], ...] },
  "cycles":  [[addr, data, "read"|"write"], ...]
}
```

Addresses/registers are decimal integers. `ram` lists only relevant cells
(including opcode bytes at `pc`). `cycles` is informational for bus timing;
ESP][ host runner validates **final register + final.ram** after one
`Cpu6502::step()`.

### Runner (do not vendor multi-MB JSON)

```text
node host/tools/sst_pack.mjs --all
  → local/apple2/forensics/sst/{9c,9e,9f,cb}.sstb   (packed, still gitignored)

host/.out/sst_undoc_runner.exe local/apple2/forensics/sst/9c.sstb
node host/tools/run_sst_undoc.mjs   # pack + run all four if present
```

CI / `npm run test:apple2` stays green **without** SST files.

`EXTERNAL_APPLEWIN_ORACLE = NOT_AVAILABLE / NOT_REQUIRED_YET`

## Handoff checkpoint (Galaxian.dsk + AppleIIGo + clean-room)

Reproduced (ROM/DSK hashes match project notes):

```text
ENTRY $C600 (--boot-disk)
HANDOFF $0842 JMP $B100
  cycle≈15773
  A=52 X=60 Y=00 SP=FD P=25
  patches B10B=38 B10C=2E B8CB=52
  zp26=00 zp27=B0 zp2B=60 zp3D=00 zp41=00
```

16/16 Track-0 sectors load and match the DSK image (page `$B1` = T0S`$0D`
except intentional boot0 patches at `$B10B/$B10C`; `$B8` differs only at
`$B8CB`).

## First bad event (natural RESET→Slot6→load→$Bxxx)

Boot0 intentionally ends with `JMP $B100` (bytes at `$0842`).

### Stage A — `RamInitMode::Zero` closed loop

Post-handoff execution stays in `$B100–$B17E`:

1. `LDX $8080,Y` loads **0** (`RamInitMode::Zero`)
2. `SEC` / `ROL $8080` briefly creates a non-zero scratch
3. Undocumented `SHY`/`SHX`/`AHX`/`SAX`/`LAX` run (all implemented)
4. `LDY $80B8,X` at **`$B13D`** loads **0** (never written; Zero fill)
5. `SHY $8080,X` at **`$B169`** with **Y=0** stores **0** (NMOS-correct)
6. `BEQ $B17E` → `$B100` always taken (Z remains set)

Earliest **control** non-progress event (Zero RAM):

```text
PC=$B17E  BEQ → $B100   (closed loop; beqNot=0 over multi-million instr)
```

Earliest **state** that locks that loop (natural Zero RAM):

```text
PC=$B13D  LDY $80B8,X   → Y stays 0
PC=$B169  SHY $8080,X   → clears decrypt scratch (Y=0 ⇒ store 0)
```

SHY with Y=0 storing 0 is correct NMOS — the defect is not “SHY should store
non-zero when Y=0”. Cold DRAM is not all zeros; Zero init is a forensic
baseline, not a claim about real power-on residue.

### Stage B — intentional motherboard ROM **data** dependency (**proven**)

With `RamInitMode::Random` (seed `0xA5A5F00D`), execution leaves `$B1xx` and
reaches `$B200`.

**Operand integrity (not corruption):**

| Check | Result |
| --- | --- |
| Bytes at `$B200` at handoff | `FB 49 D5 D0 F4 EA BD 8C C0 …` |
| Same bytes on `Galaxian.dsk` | **FOUND** Track 0 sector `$0B` offset 0 |
| SMC to `$B2xx` before first `ISB` | **0** |
| Bytes at first `ISB` | identical → **INTACT_FROM_HANDOFF** |

Disassembly of the intact loader fragment:

```text
$B200  FB 49 D5   ISB $D549,Y
$B203  D0 F4      BNE $B1F9
$B206  EA         NOP
$B207  BD 8C C0   LDA $C08C,X    ; Disk II data latch
$B20A  10 FB      BPL *-3
$B20C  C9 AA      CMP #$AA       ; nibble sync
```

Observed at first `ISB`: `Y=$9E` → EA=`$D5E7`. Bus spy (Random, post-handoff):

| Kind | Count |
| --- | --- |
| ROM **data** reads | 142 (140× `$D5E7` from `$B200`; 2× `$D971` from `$B13B`) |
| ROM **code** fetches | **0** |
| ROM write attempts | 564 (RMW stores ignored) |

AppleIIGo at `$D500–$D5FF`: **all `$00`** (no Applesoft image). Published Apple
II+ memory maps place Applesoft in `$D000–$F7FF`; the access is therefore a
legitimate **ROM-data** use (fingerprint / key stream for the decrypt loop),
not an accidental bad address and not a ROM code call.

```text
CLASSIFICATION = APPLEIIGO_MISSING_REQUIRED_DATA
GATE           = BLOCKED_REQUIRES_USER_APPLE_II_PLUS_ROM
romDependency  = USER_APPLE_II_PLUS_ROM
reason         = ROM_DATA
```

**Forbidden:** embedding `$D5xx` tables, Galaxian-specific read intercepts, or
reconstructed Applesoft bytes. Correct path: user-supplied 12288-byte Apple II+
ROM (`local/roms/`, device `/esp2/roms/system.rom`).

Tool: `host/.out/rom_data_forensic.exe` (local maps under
`local/apple2/forensics/rom_data_*`, gitignored).

## Generic CPU fix this pass (ISB RMW on write-ignored targets)

Upstream fake6502 `isb()` was `inc(); sbc();`. After `putvalue`, `sbc`’s
`getvalue()` re-reads memory. On Apple II motherboard ROM, writes are ignored,
so the secondary SBC wrongly used the **original** byte.

NMOS ISB/ISC must SBC with the **ALU-incremented** byte even when the store is
discarded. Same one-shot override applied to DCP/SLO/RLA/SRE/RRA.

Regression: `testIsbWriteIgnored` / `testIsbRam` in `test_cpu_main.cpp`.

This fix is correct and kept; it does **not** unblock Galaxian under AppleIIGo
because `$D5E7` is zero either way and the loop’s exit condition needs real
Applesoft bytes.

## Undocumented opcodes in post-handoff window

| Opcode | Role | Status |
| --- | --- | --- |
| `$80` | NOP #imm | OK |
| `$83/$87/$8F` | SAX | OK |
| `$9C` | SHY | implemented (base `eabasehi` / page-cross rewrite) |
| `$9E` | SHX | implemented |
| `$93/$9F` | AHX | implemented |
| `$B3/$BF` | LAX | OK |
| `$FB` | ISB abs,Y | RMW-on-ROM ALU fix (this pass) |
| KIL | — | none observed |

SMC in window: SHX/AHX write into `$Bxxx` (e.g. `$B3B3`).

## Differential vs Apple ][js (fpbasic + Galaxian)

Inputs matched (HOST):

| Asset | SHA-256 |
| --- | --- |
| `local/roms/system.rom` (fpbasic) | `378ba00c…fc346249` |
| `Galaxian.dsk` | `b1e85b78…132c2d1` |

### Apple ][js Disk II

- Module: `js/roms/cards/disk2.ts` → `BOOTSTRAP_ROM_16` (256 bytes)
- Head: `A2 20 A0 00 A2 03 …` (classic Disk II PROM)
- Autostart signature: `$Cn01=$20`, `$Cn03=$00`, `$Cn05=$03` — **matches**
- Local forensic copy (gitignored): `local/roms/diskii_apple2js_16.prom`

### Apple ][js RAM init

`js/util.ts` `allocMem` (AppleWin): `(addr&2)?0x00:0xFF` + sparse random
cells. Cold `$8080=$FF`.

ESP][ now exposes `RamInitMode::DramAppleWin` (deterministic sparse garbage).

### Natural boot paths

| Path | Result |
| --- | --- |
| Apple ][js | `RESET $FA62` → Autostart → Slot 6 PROM (sig OK) |
| ESP][ CleanRoom | `$Cn00=JMP $C800` — **no** Autostart sig → Applesoft |
| ESP][ + apple2js PROM | `RESET $FA62` → **HANDOFF** `$0842 JMP $B100` (regs A=52 X=60 Y=00 SP=FD) |

CleanRoom vs real PROM handoff **registers match**; first relevant post-handoff
divergence is **not** PROM handoff state when using the apple2js PROM.

### First relevant divergence — Disk II address-field sector ID (CP4 handoff)

Same `Galaxian.dsk` + same fpbasic + same Slot-6 PROM bytes. At
`$0842 JMP $B100` both machines agree on `$B000` (= T0S0), but **`$B100` differed**
while ESP][ still put **DOS logical** IDs into Address Fields (bug):

| | Apple ][js (headless) | ESP][ (pre-fix) |
| --- | --- | --- |
| `$B100` head | `A9 00 85 F7` (**T0S1**) | `80 80 80 BE` (**T0S13**) |
| `$B700` head | `4B 4F 53 57` (**T0S7**) | `A9 00 85 F7` (**T0S1**) |
| Page map `$Bn00` | logical S0… via `_DO` Address IDs | table values treated as logical |
| SP @ handoff | `$FF` | `$FD` |
| cycles @ handoff | ~2.37e6 | ~4.16e6 |

Galaxian boot0 sector table at `$0845` (from T0S0):

```text
00 0D 0B 09 07 05 03 01 0E 0C 0A 08 06 04 02 0F
```

This is apple2js `_DO[]`: **index = DOS logical**, **value = Address Field
(physical) sector ID** to request. Boot0 loads that list into `$B000+`.

Authoritative Disk II / DOS 3.3 semantics (Beneath Apple DOS; AppleWin
`NibblizeTrack`; apple2js `createDiskFromDOS`; AppleII_Esp32 `DOS33Skew`):

- Address Field sector ID = **physical** rotational slot `0..15`
- Data Field = DSK/DO file slot `DO[physical]` (= DOS logical)

Therefore Address Field `$0D` → `DO[13]=1` → **T0S1** on real hardware.
Apple ][js matches hardware. Pre-fix ESP][ put logical IDs in Address Fields
and was **wrong** (`ESP2_DISK_MAPPING_BUG`). Fix: `DiskIITrackBuilder::buildTrack`
writes physical IDs (see `docs/apple2/disk-ii.md`).

```text
CLASSIFICATION = ESP2_DISK_MAPPING_BUG
GATE           = ADDRESS_FIELD_MUST_BE_PHYSICAL
REAL_HW_$0D    = T0S1 (DSK slot 1)
APPLE2JS       = HARDWARE_CORRECT (not a quirk)
APPLEII_ESP32  = PHYSICAL_DISK_SEMANTICS (media = different release)
```

Before JMP, boot0 patches `$B10B=$38` / `$B10C=$2E` / `$B8CB=$52` (SEC/ROL
entry). With correct `$B100` (= T0S1 cleartext), Stage-A encrypted blob is **not**
what runs at `$B100`.

### Secondary — Stage A / `$8080` (artifact of wrong `$B100` = T0S13)

With pre-fix T0S13 at `$B100`, Stage A ran:

```text
$B104  BE 80 80   LDX $8080,Y
$B10B  38         SEC          ; patched by boot0
$B10C  2E 80 80   ROL $8080
$B134  B0 xx      BCS …
$B13D  BC B8 80   LDY $80B8,X
```

DramAppleWin / AppleWin cold `$8080=$FF` is a **ROL fixed point** (`SEC;ROL`
keeps `$FF` and C=1) → permanent `$B134`/`$B0F5` oscillation, never `$B200`.

With `$8080≠$FF` and non-zero `$80B8,X`, Stage A can reach `$B200`, then the
`ISB $D549,Y` / `BNE $B1F9` loop runs with **frozen Y** (loop body does not
modify Y; exit needs SBC Z). Still no HGR on HOST with current Random /
DramAppleWin hybrids.

```text
SECONDARY = STAGE_A_BCS_8080 / STAGE_B_ISB_EXIT
```

Do **not** force Galaxian loop exits or patch `$8080` / `$B100` as a title hack.

## Clean-room PROM handoff (generic defect fixed earlier)

DOS boot0 uses ZP `$41` as track. Clean-room T0S0 exit previously relied on
power-on zeros. With `$FF` RAM fill, first sector request used `track=$FF` and
hung in `$C905`.

Fix: after T0S0, set **`$41=0`** and **`$3D=0`** before `JMP $0801`.

## Secondary experiments (not fixes)

| Perturbation | Result |
| --- | --- |
| Force Y=`$FF` at handoff | Leaves `$B1xx` into `$B2xx`, then stalls |
| Seed only `$80B8=$FF` | Falls through; hits **`BRK` at `$B1BE`** → IRQ/BRK vector `$FA62` |
| `RamInitMode::Ones` | Leaves `$B1xx` briefly; stays in `$B0/$B1`; no HGR game paint |
| Force entry `$B700` | Prior session: JSR `$B706` stack growth without RTS |

## Post-mapping HOST production check (RamInitMode::Zero)

After Address-Field fix (`c9723a9`), Galaxian with production RAM (`Zero`):

| Gate | Result |
| --- | --- |
| Handoff `$B100`/`$B700` | T0S1 / T0S7 — **OK** |
| First HGR | yes (`page2`, cracktro paint) |
| Visible framebuffer | **"Beautiful Boot / by Mini Appler"** only |
| Galaxian title/playfield | **NO** — stuck |

Loader seeks outward after splash, then **oscillates forever** between
quarter-tracks **65 ↔ 66** (T16.25 ↔ T16.50), PC hot in `$BBEB` / `$BA7E`
(`LDA $C08C,X` / `BPL` wait) and delay `$BC04`. Motor stays on. Lit HGR
pixels remain ~527 for >60M cycles. Keyboard does not unblock.

```text
CLASSIFICATION = GALAXIAN_STUCK_STEPPER_HALFTRACK_HUNT
ROOT_CAUSE     = stepper advanced +1 qt per adjacent phase ON (should be +2 qt)
FIX            = PHASE_DELTA quarter-track table (apple2js / Disk II half-track)
HOST_PASS      = YES (menu → key A → Galaxian playfield/attract)
```

Pre-fix: adjacent phase ON moved **one quarter-track**. Real Disk II / apple2js /
AppleII_Esp32 advance **one half-track** (two quarter-tracks) per adjacent coil.
That undershoot produced AF-seek hunting that settled into qt **65↔66**.

Post-fix (`RamInitMode::Zero`): full **Beautiful Boot** menu (A–D), then after
`$C1` (‘A’) load completes → HGR Galaxian SCORE/HISCORE/formation/SHIPS.

Local frames (gitignored): `galaxian_prod_settled.png`, `galaxian_title.png`.

## Physical

HOST_PASS frozen (`GALAXIAN_HOST = PASS`). Physical path uses the same generic
Disk-II fixes (`c9723a9`, `021be3e`) plus Windows CDC input bridge for
keyboard/gamepad during bring-up — see `docs/architecture/input-providers.md`.
