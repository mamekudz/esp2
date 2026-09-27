# Local Apple II media workflow (`local/apple2/`)

ESP][ **never** stores downloaded Apple II ROM/game media in tracked Git
content. All acquisition lives under an explicitly gitignored root:

```
local/apple2/
  roms/         # redistributable ROMs (apple2:rom:sync)
  disks/        # normalized runtime images (apple2:media:prepare)
  cache/        # original downloaded sources (apple2js JSON, …)
  manifests/    # local path/hash maps (ignored)
  user/         # optional user-supplied originals
```

`.gitignore` rule:

```
/local/apple2/**
!/local/apple2/README.md
```

## Important

**Downloaded ≠ redistributable.**

Fetching a title from the Apple ][js / scullinsteel website for local
compatibility testing does **not** grant redistribution rights. Cache is
`LOCAL_TEST_ONLY` unless a title is independently classified
`REDISTRIBUTABLE`. Downloading **never upgrades** provenance
(`USER_SUPPLIED_ONLY` stays `USER_SUPPLIED_ONLY`).

## Example flow (Galaxian)

```bash
gulp apple2:rom:sync
gulp apple2:media:sync --title galaxian
gulp apple2:media:prepare --title galaxian
gulp apple2:compat --test galaxian --run
gulp apple2:device:sync --title galaxian --port COM5
```

Device sync uploads **only** the selected title’s runtime assets (ROM + disk),
never the complete catalog cache.

On this board, catch the firmware boot upload window shortly after
`pio run -e apple2_text -t upload` (or a clean app restart). Mid-run
`#ESP2UPLOAD` may time out if CDC/SD ownership is busy; prefer
`--listen-ms 20000 --chunk 256` for large disks.

## Tasks

| Task | Purpose |
| --- | --- |
| `apple2:rom:sync` | Download only catalogued redistributable ROMs (AppleIIGo PD) |
| `apple2:media:sync --title X` | Fetch one apple2js title into `cache/` (+ prepare when safe) |
| `apple2:media:sync --all` | Complete LOCAL_TEST_ONLY cache |
| `apple2:media:prepare --title X` | Offline normalize → `disks/` (prefer DSK; no forced DSK for NIB) |
| `apple2:media:status` / `list` / `audit` | Library overview |
| `apple2:media:clean` | Remove generated `disks/` (not `user/` unless `--user`) |
| `apple2:compat --test X` | Host harness; resolves ignored media via local manifest |
| `apple2:device:sync --title X --port COMx` | Serial upload of required runtime only |

Underlying apple2js tooling remains: `apple2js:sync` / `catalog` / `audit`,
`media:identify` / `media:import`.

## Git / backup safety

- Before every download, tooling runs `git check-ignore` on the destination and
  **ABORT**s if not ignored.
- `backup:git` lists `local/apple2` in `GIT_BACKUP_NEVER_STAGE` (never stages).
- NAS `backup` / `backup:all` **do** include `local/apple2/` (and `local/roms/`)
  as private backup — Git eligibility ≠ NAS eligibility.
  See `docs/tooling/backup.md`.

## Tracked vs local manifests

| Tracked (metadata only) | Local (ignored) |
| --- | --- |
| `config/apple2/roms-catalog.json` | `local/apple2/manifests/local-library.json` |
| `config/apple2/media-catalog.json` | cache/disks paths + hashes |
| `compatibility/tests/*.json` | |

Tracked files must never contain media payloads or base64 disk contents.
