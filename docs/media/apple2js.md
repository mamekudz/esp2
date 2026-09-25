# apple2js and ESP][

Status: **DEVELOPMENT REFERENCE / CATALOG SOURCE** — not affiliated.

Upstream:

- Repository: https://github.com/whscullin/apple2js (MIT for the **emulator**)
- Website: https://www.scullinsteel.com/apple2/

ESP][ is **not** affiliated with apple2js or Will Scullin.

## Critical license distinction

The MIT license covers the apple2js **software**. It does **not** grant rights to
redistribute third-party Apple II disk images that appear on the website or
that were historically converted with `bin/dsk2json`.

Default media classification: **UNKNOWN** unless explicit evidence exists.

## What lives where (inspected)

Pinned local cache (gitignored): `.cache/apple2js/`

| Location | Content |
| --- | --- |
| Git `json/disks/` on `main` | Small set (blank/system/utility JSON disks + `index.json`) |
| Website `json/disks/index.json` | Full UI catalog (~261 titles including commercial games) |
| Website `json/disks/*.json` | Base64 track/sector (or nibble) **media blobs** — do not commit |

Inspected upstream commit is recorded by `gulp apple2js:sync` into
`.cache/apple2js-meta/upstream-pin.json`.

This milestone inspected git `main` at:

    ee0aed25f73c69d0245e86a2a5fccb3324c3056c

## JSON disk format

Produced by `bin/dsk2json`:

```json
{
  "name": "...",
  "category": "...",
  "type": "dsk|po|nib|...",
  "encoding": "base64",
  "data": [ /* tracks */ ]
}
```

- **dsk/po**: `data[track=0..34][sector]` = base64 of 256-byte sector
- **nib**: `data[track]` = base64 of 0x1A00 nibble bytes
- Optional: `readOnly`, `disk`, `2e`, `private` (private skipped by `bin/index`)

Host converter prototype: `dev/tools/apple2js/json_disk.mjs`
(`jsonDiskToFlatImage`). Conversion ≠ permission to redistribute.

## Catalog pipeline

```
apple2js index (git and/or website metadata)
        |
  audit (redistribution status)
        |
  normalize → ESP][ game metadata (game.json + provenance)
        |
  user media (optional) → SD library path
```

Developer tasks (network only when named `sync`):

    gulp apple2js:sync
    gulp apple2js:catalog
    gulp apple2js:audit
    gulp media:identify
    gulp media:import

Normal `build` / `test:host` / `docs` never contact GitHub or scullinsteel.com.

## Choplifter / Night Mission

Both appear on the **website** catalog:

- `json/disks/choplifter.json` — Game
- `json/disks/nightmission.json` — Game

Neither is present in the public git `json/disks/` tree on the inspected
`main` tip. Classification: **USER_SUPPLIED_ONLY**. Metadata stubs may exist
under `library/metadata/`; disk images are never committed.

## Oracle use

See `docs/apple2/apple2js-oracle.md` for comparing ESP][ behavior against
apple2js during development (video/disk/joystick) without copying code.
