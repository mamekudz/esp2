# SD / library layout (ESP][)

Metadata and media are separate. A catalog entry without a disk file is
**valid** (`mediaStatus: not_installed`).

## On-device (microSD)

```
/esp2/
  disks/
    Esp2BootTest.dsk       # project-owned Level-4 boot (PART E)
    Esp2BootTest.po        # optional same payload, PO order
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

Firmware may also seed `/esp2/disks/Esp2BootTest.dsk` once from
`generateEsp2BootTestImage` if the file is missing (project-owned only).

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
