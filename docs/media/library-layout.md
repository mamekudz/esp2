# SD / library layout (ESP][)

Metadata and media are separate. A catalog entry without a disk file is
**valid** and should surface as media-not-installed in the UI later.

## On-device (microSD) convention

```
/apple2/
  games/
    <game-id>/
      game.json          # metadata (may ship without media)
      disk1.dsk          # user-supplied (optional)
      disk2.dsk          # optional
      cover.png          # optional
```

Example:

```
/apple2/games/choplifter/game.json          # USER_SUPPLIED_ONLY metadata
/apple2/games/choplifter/disk1.dsk          # only if the user installs it
```

## In-repo (development)

```
library/
  metadata/                 # provenance-bearing metadata stubs (no media)
  user/                     # local import target (gitignored if desired)
fixtures/synthetic/library/ # legal synthetic fixtures only
```

`library/user/` should remain local; do not commit commercial images.

## game.json media fields

- `disks[].mediaInstalled` — boolean for the local/SD file
- `mediaStatus` — `not_installed` | `installed` | `available_if_obtained`
- `source` — apple2js provenance block when imported
- `redistribution.status` — language-neutral enum

UI copy for missing media belongs in i18x (e.g. `media.not_installed`), not
in core logic.
