# SD / library layout (ESP][)

Metadata and media are separate. A catalog entry without a disk file is
**valid** (`mediaStatus: not_installed`).

## On-device (microSD)

```
/apple2/
  catalog/                 # metadata stubs / index (optional)
  games/
    <game-id>/
      game.json
      disk1.dsk            # user-supplied when needed
      disk2.dsk
  diagnostics/             # future self-test disks
```

Host prepare:

    gulp sd:prepare --target <dir>          # dry-run by default
    gulp sd:prepare --target <dir> --dryRun=false

Never formats a drive; refuses bare drive roots; skips existing files unless
`confirmOverwrite=true`.

## Host library

```
library/
  metadata/                # provenance stubs (committed)
  user/games/<id>/         # local imports (gitignored via library/user/)
fixtures/synthetic/        # legal test media
fixtures/redistributable/  # clearly licensed third-party fixtures
```

## Import

    gulp media:identify --file game.dsk
    gulp media:import --file game.dsk
    gulp media:import --file disk2.dsk --game-id choplifter --title Choplifter

Never downloads commercial images because a catalog hash/title matched.
