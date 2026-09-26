# Local Apple II media root

This directory holds **downloaded and runtime** Apple II ROM/disk media for
local development and compatibility testing.

**Everything under this tree is gitignored** (except this README).

```
local/apple2/
  roms/         # redistributable ROMs from apple2:rom:sync (e.g. AppleIIGo)
  disks/        # normalized ESP][ runtime images from apple2:media:prepare
  cache/        # original downloaded source (apple2js JSON, etc.)
  manifests/    # local path/hash maps (ignored — may contain absolute paths)
  user/         # optional user-supplied originals (never deleted by default clean)
```

## Policy

- **Downloaded ≠ redistributable.** Website availability does not grant
  redistribution rights. Local cache is `LOCAL_TEST_ONLY` unless a title has
  independently verified rights (`REDISTRIBUTABLE`).
- Do not commit ROM/disk payloads. Tracked manifests live under `config/apple2/`
  and `compatibility/tests/` (metadata only).
- Not included in NAS `backup` / `backup:all`.

## Workflow

```bash
gulp apple2:rom:sync
gulp apple2:media:sync --title galaxian
gulp apple2:media:prepare --title galaxian
gulp apple2:compat --test galaxian --run
gulp apple2:device:sync --title galaxian --port COM5
```

See `docs/media/local-apple2-workflow.md`.
