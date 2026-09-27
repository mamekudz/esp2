# ESP][ µGulp task history

Machine-readable companion: [`gulp-task-manifest.json`](./gulp-task-manifest.json).

This document records the historical inventory of first-party Gulp / µGulp tasks
so future recovery does **not** depend on guessing which commit was “good”.

## Audit method

- Inspected every commit that touched `gulpfile.mjs` from
  `af155ef` (Phase-1 baseline) through `aca630c` (encoding fix).
- Extracted `gulp.task("…")` registrations and `_Tag(…, { gulpName })` IDs.
- Raw audit dump: `_gulp_history_raw.json` (generated, not authoritative).

## Important finding

**No first-party task IDs disappeared from the ESP][ `gulpfile.mjs` history.**

The union of all historical task IDs grew monotonically:

| Commit   | Count | Note |
|----------|------:|------|
| `af155ef` | 12 | Firmware + Docs/Backup baseline |
| `8f567c5` | 17 | + apple2js / media |
| `caba32d` | 19 | + media:inspect, sd:prepare |
| `43e9c8b` | 21 | + docs:en-US / docs:de-DE |
| `5cd1738` | 24 | + apple2 host/ROM |
| `4ab0d32` | 25 | + apple2:disk-test |
| `2c3698e` | 26 | + apple2:compat |
| `52357f4` | 28 | + device:upload / usb-storage — **incomplete baseline myth** |
| `b886473` | 36 | + Apple II local media workflow |
| `aca630c` | 36 | encoding fix only; same 36 IDs |

`52357f4` (28 tasks) must **not** be treated as the maximum legitimate set.
It predates the Apple II local-media tasks and never contained Watchy-family
aliases that later UX expected (`git:status`, `backup:list`, …).

## Why Git / NAS / Firmware looked “missing”

1. **µGroup collapse** — `Docs & Backup` started **collapsed**, hiding
   `backup:git`, `backup`, `backup:all`, and docs tasks in the µGulp UI.
2. **Naming / group flattening** — Git checkpoint lived under Docs & Backup
   rather than a visible **Git** group; NAS list/verify aliases were never
   registered as first-party gulp tasks (only `backup` + npm `backup:nas`).
3. **Encoding regression (`b886473`)** — double-UTF-8 `µ` → `ÃÂµ` killed
   metadata until `aca630c`. Task IDs remained; display names / groups did not.

## Classification legend

| Class | Meaning |
|-------|---------|
| `ACTIVE_CURRENT` | Present in canonical manifest |
| `ACCIDENTALLY_LOST` | Was registered, then vanished without deprecation evidence |
| `INTENTIONALLY_REPLACED` | Replaced by a newer ID with evidence |
| `INTENTIONALLY_REMOVED` | Removed on purpose with evidence |
| `RENAMED` | Same intent, new technical ID |
| `FAMILY_RESTORED` | Watchy-family / package-script intent restored as first-party task |
| `UNKNOWN` | Insufficient evidence |

## Historical ESP][ task IDs (mono growth)

All IDs below are `ACTIVE_CURRENT` unless noted.

### Firmware (`af155ef`+)

| ID | Introduced | Class | Notes |
|----|------------|-------|-------|
| `build` | `af155ef` | ACTIVE_CURRENT | env-aware via `ESP2_PIO_ENV` / `firmwareEnv()` |
| `flash` | `af155ef` | ACTIVE_CURRENT | build + upload |
| `upload` | `af155ef` | ACTIVE_CURRENT | |
| `size` | `af155ef` | ACTIVE_CURRENT | |
| `clean` | `af155ef` | ACTIVE_CURRENT | |
| `rebuild` | `af155ef` | ACTIVE_CURRENT | series(clean, build) |
| `firmware:env` | catalog recovery | FAMILY_RESTORED | shows active PIO env |

### Tools

| ID | Introduced | Class |
|----|------------|-------|
| `devices` | `af155ef` | ACTIVE_CURRENT |
| `monitor` | `af155ef` | ACTIVE_CURRENT |
| `format:check` | catalog recovery | FAMILY_RESTORED |
| `i18x:gulp` | catalog recovery | FAMILY_RESTORED |

### Docs

| ID | Introduced | Class | Notes |
|----|------------|-------|-------|
| `docs` | `af155ef` | ACTIVE_CURRENT | group split from Docs & Backup |
| `docs:en-US` | `43e9c8b` | ACTIVE_CURRENT | |
| `docs:de-DE` | `43e9c8b` | ACTIVE_CURRENT | |

### Git

| ID | Introduced | Class | Notes |
|----|------------|-------|-------|
| `backup:git` | `af155ef` | ACTIVE_CURRENT | checkpoint commit + optional push |
| `git:status` | catalog recovery | FAMILY_RESTORED | dry-run of checkpoint rules |
| `git:commit` | catalog recovery | FAMILY_RESTORED | checkpoint without push |
| `git:push` | catalog recovery | FAMILY_RESTORED | push only |

### Backup / NAS

| ID | Introduced | Class | Notes |
|----|------------|-------|-------|
| `backup` | `af155ef` | ACTIVE_CURRENT | NAS form; excludes `local/` |
| `backup:all` | `af155ef` | ACTIVE_CURRENT | docs + git + NAS |
| `backup:nas` | catalog recovery | FAMILY_RESTORED | was npm alias → first-party |
| `backup:list` | catalog recovery | FAMILY_RESTORED | read-only destination check |
| `backup:verify` | catalog recovery | FAMILY_RESTORED | essential-file verify; no restore |

### Media / apple2js

| ID | Introduced | Class |
|----|------------|-------|
| `apple2js:sync` | `8f567c5` | ACTIVE_CURRENT |
| `apple2js:catalog` | `8f567c5` | ACTIVE_CURRENT |
| `apple2js:audit` | `8f567c5` | ACTIVE_CURRENT |
| `media:identify` | `8f567c5` | ACTIVE_CURRENT |
| `media:import` | `8f567c5` | ACTIVE_CURRENT |
| `media:inspect` | `caba32d` | ACTIVE_CURRENT |
| `sd:prepare` | `caba32d` | ACTIVE_CURRENT |

### Apple II (host / media / device)

| ID | Introduced | Class |
|----|------------|-------|
| `apple2:rom-identify` | `5cd1738` | ACTIVE_CURRENT |
| `apple2:rom-test` | `5cd1738` | ACTIVE_CURRENT |
| `apple2:host` | `5cd1738` | ACTIVE_CURRENT |
| `apple2:disk-test` | `4ab0d32` | ACTIVE_CURRENT |
| `apple2:compat` | `2c3698e` | ACTIVE_CURRENT |
| `apple2:rom:sync` | `b886473` | ACTIVE_CURRENT |
| `apple2:media:sync` | `b886473` | ACTIVE_CURRENT |
| `apple2:media:prepare` | `b886473` | ACTIVE_CURRENT |
| `apple2:media:status` | `b886473` | ACTIVE_CURRENT |
| `apple2:media:list` | `b886473` | ACTIVE_CURRENT |
| `apple2:media:audit` | `b886473` | ACTIVE_CURRENT |
| `apple2:media:clean` | `b886473` | ACTIVE_CURRENT |
| `apple2:device:sync` | `b886473` | ACTIVE_CURRENT |

### Device storage

| ID | Introduced | Class |
|----|------------|-------|
| `device:upload` | `52357f4` | ACTIVE_CURRENT |
| `device:usb-storage` | `52357f4` | ACTIVE_CURRENT |

### Tests

| ID | Introduced | Class |
|----|------------|-------|
| `test:infra` | catalog recovery | FAMILY_RESTORED |
| `test:mugulp-catalog` | catalog recovery | FAMILY_RESTORED |
| `test:host` | catalog recovery | FAMILY_RESTORED |

## Accidentally lost (`ACCIDENTALLY_LOST`)

**None** in ESP][ `gulpfile.mjs` history (commit `af155ef`…`aca630c`).

UX loss was group collapse + missing family aliases, not deletion of IDs.

## Intentionally obsolete / not restored

| Candidate | Evidence | Decision |
|-----------|----------|----------|
| Help / clutter tasks | Early draft explicitly removed help clutter | NOT RESTORED |
| Dual NAS function-object alias sharing one `_Tag` | Early bug; fixed by separate `backup` / `backup:nas` functions | INTENTIONALLY_REPLACED by distinct tagged tasks |
| Hard-coded `PIO_ENV = "bringup"` only | Replaced by `ResolvePioEnv` / `ESP2_PIO_ENV` | INTENTIONALLY_REPLACED |

## Inventory snapshots (A / B / C)

### A — `52357f4` (mythical “complete” baseline): **28** tasks

Incomplete: missing Apple II local-media set and family Git/NAS aliases.

### B — `aca630c` (encoding-fixed): **36** tasks

Complete for historical ESP][ IDs; Git/NAS still grouped under collapsed
**Docs & Backup**; no `git:status` / `backup:list` / `backup:verify` /
`firmware:env` / test tasks.

### C — recovered canonical catalog: **48** tasks

See `gulp-task-manifest.json`. Adds family workflow visibility while
preserving all Apple II tasks from `b886473` / `aca630c`.

## Group hierarchy (current)

```
Firmware          (open)
Tests             (collapsed)
Tools             (open)
Docs              (collapsed)
Git               (open)
Backup            (open)
Media / apple2js  (collapsed)
Apple II/Media    (open)
Apple II/Emulator (collapsed)
Apple II/Compatibility (collapsed)
Apple II/Device   (collapsed)
Device Storage    (collapsed)
```

## Safety rules preserved

- `GIT_BACKUP_NEVER_STAGE` includes `local/apple2`, `local/roms`, `library/user`, secrets.
- NAS backup **includes** `local/apple2`, `local/roms`, `library/user`, `_refs`,
  `3dprint` (private). Disposable caches (`node_modules`, `.pio`, …) stay excluded.
  See `docs/tooling/backup.md`.
- `git:status` is dry-run only; tests must not create real commits.
- `backup:verify` / `backup:list` are read-only (no restore).
