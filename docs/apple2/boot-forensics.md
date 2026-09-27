# Apple II boot forensics (HOST)

Status: **HOST diagnostic tooling** for post-disk-load CPU/boot failures.

## Tool

```text
host/.out/boot_forensic.exe
  --rom local/apple2/roms/appleiigo.rom
  --disk local/apple2/disks/<title>.dsk
  --boot-disk
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

Post-handoff execution stays in `$B100–$B17E`:

1. `LDX $8080,Y` loads **0** (`RamInitMode::Zero`)
2. `SEC` / `ROL $8080` briefly creates a non-zero scratch
3. Undocumented `SHY`/`SHX`/`AHX`/`SAX`/`LAX` run (all implemented)
4. `LDY $80B8,X` at **`$B13D`** loads **0** (never written; Zero fill)
5. `SHY $8080,X` at **`$B169`** with **Y=0** stores **0** (NMOS-correct)
6. `BEQ $B17E` → `$B100` always taken (Z remains set)

Earliest **control** non-progress event:

```text
PC=$B17E  BEQ → $B100   (closed loop; beqNot=0 over multi-million instr)
```

Earliest **state** that locks that loop (natural Zero RAM):

```text
PC=$B13D  LDY $80B8,X   → Y stays 0
PC=$B169  SHY $8080,X   → clears decrypt scratch (Y=0 ⇒ store 0)
```

Secondary experiments (not fixes):

| Perturbation | Result |
| --- | --- |
| Force Y=`$FF` at handoff | Leaves `$B1xx` into `$B2xx`, then stalls |
| Seed only `$80B8=$FF` | Falls through; hits **`BRK` at `$B1BE`** → IRQ/BRK vector `$FA62` (crash, not title) |
| `RamInitMode::Ones` after `$41=0` PROM fix | Boots past handoff; still no HGR; stays in `$B0/$B1` |
| Force entry `$B700` | Prior session: JSR `$B706` stack growth without RTS |

SHY with Y=0 storing 0 is correct NMOS — the defect is not “SHY should store non-zero when Y=0”.

## Undocumented opcodes in post-handoff window

| Opcode | Role | Status |
| --- | --- | --- |
| `$80` | NOP #imm | OK |
| `$83/$87/$8F` | SAX | OK |
| `$9C` | SHY | implemented (base `startpage` high-byte) |
| `$9E` | SHX | implemented |
| `$93/$9F` | AHX | implemented this milestone |
| `$B3/$BF` | LAX | OK |
| KIL | — | none observed |

SMC in window: SHX/AHX write into `$Bxxx` (e.g. `$B3B3`); with X=0 those stores write 0.

## Clean-room PROM handoff (generic defect fixed)

DOS boot0 uses ZP `$41` as track. Clean-room T0S0 exit previously relied on
power-on zeros. With `$FF` RAM fill, first sector request used `track=$FF` and
hung in `$C905`.

Fix: after T0S0, set **`$41=0`** and **`$3D=0`** before `JMP $0801`.

## Proprietary Apple ROM

Natural post-handoff window: **no** `$D000–$FFFF` instruction fetches.

```text
ORIGINAL_APPLE_ROM_NOT_YET_IMPLICATED
```

(The `$80B8=$FF` BRK→`$FA62` path is a crash into the BRK/IRQ vector, not a
game ROM call.)

## Physical

Out of scope for this forensic milestone.
