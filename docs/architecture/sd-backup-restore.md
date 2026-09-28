# ESP][ microSD backup / restore

## Purpose

Logical, hash-verified backup and restore of the ESP][ runtime tree on
microSD. Primary uses:

- card replacement and capacity migration (e.g. 256 GB → 8 GB when data fits)
- recovery after SD failure or corruption
- private preservation of local ROM / media / config assets

This is **not** a raw sector image of the whole card. Unused capacity is not
copied.

## Strategy: logical filesystem tree

| | |
| --- | --- |
| Backed up | Entire `/esp2/` tree (files and directories, including unknown user content) |
| Not backed up | Free space / unused sectors outside `/esp2/` |
| Format | Timestamped directory under `local/sd-backups/` |

Byte-exact copies: no newline conversion, JSON reformatting, or media/ROM
transforms.

## Filesystem requirements

ESP][ firmware mounts the card with the Arduino/`SD` FAT stack
(`SD.begin` / FatFs). In practice:

| Expectation | V1 policy |
| --- | --- |
| Supported host FS for MSC access | **FAT32** (typical); exFAT may work on the host but is not guaranteed for firmware |
| Partition layout | Single usable volume with an `esp2/` directory at the volume root |
| Format on restore | **Never automatic.** Destination must already have a compatible filesystem. Formatting requires a separate, explicit user action |

Card nominal capacities may differ. Only total backed-up bytes vs destination
free/usable space matter.

## Backup layout

```
local/sd-backups/
  <YYYY-MM-DD_HHMMSS>/
    .incomplete          # present until verification finishes
    manifest.json
    files/
      esp2/
        config/
        roms/
        disks/
        …                # any other paths under esp2/
```

Each backup is self-contained. Older backups are never overwritten by default.

### Manifest (schema version 1)

Machine-readable `manifest.json` includes at least:

- `schemaVersion`, `product` (`ESP][`), `kind` (`logical-esp2-tree`)
- `complete: true` only for finished backups
- `createdAt`, optional `projectVersion` / `gitCommit`
- source volume metadata (filesystem, capacity/free when available) — **no**
  host-absolute restore-critical paths
- `totals` (files, directories, bytes)
- per-file relative path, size, SHA-256

All file paths in the manifest are relative to the backup `files/` root
(e.g. `esp2/config/system.json`). Absolute or `../` paths are rejected.

Incomplete backups (`.incomplete` present or `complete !== true`) are not
restorable.

## Git / NAS policy

| Property | `local/sd-backups/` |
| --- | --- |
| Git | **Ignored** (except `README.md`); listed in `GIT_BACKUP_NEVER_STAGE` |
| NAS | **Included** in normal project NAS backup (`NAS_BACKUP_LOCAL_ASSET_DIRS`) |

SD backups may contain proprietary ROMs and private config. Do not commit them.
A finished SD backup does **not** auto-start a NAS run; the next normal NAS
backup includes this directory.

Restore works from any copy of a complete backup directory (project tree or
NAS-restored location). Manifests must not require the original drive letter.

## µGulp UI (exactly two Device tasks)

| Task ID | en-US | de-DE |
| --- | --- | --- |
| `device:sd:backup` | Back up SD card V\<version/\> | SD-Karte sichern V\<version/\> |
| `device:sd:restore` | Restore SD card V\<version/\> | SD-Karte wiederherstellen V\<version/\> |

Both live under the existing **Device** group (collapsed by default with other
groups). No visible helper tasks for verify / list / hash / mount.

### Backup form

Normal fields:

- **ESP][ device / COM port** — optional; empty → `ESP2_PORT` / AskPort
- **Backup destination** — defaults to `local/sd-backups/`
- **Source** — default *ESP][ device (USB Storage)*; advanced manual mount is fallback only

Opening the form does **not** enter MSC or copy files.

After Start: connect → ENTER USB Storage → wait for Windows mount (before/after
volume diff) → require `/esp2/` on backup candidates → copy + SHA-256 → leave
MSC. Ambiguous new volumes prompt a selection dialog. Card capacity alone is
never used as identity.

Prefer TinyUSB MSC when available (`ARDUINO_USB_MODE=0`). On the default HW CDC
build, MSC may be unavailable — use **Advanced: manually select mounted SD**
(card reader). See `docs/storage/usb-storage.md`.

### Restore form

Normal fields:

- **Backup** / **Backup folder** (default browser root `local/sd-backups/`)
- **ESP][ device / COM port**
- **Destination** — default USB Storage; advanced manual mount optional
- Optional dry run

Opening the form does **not** write the SD.

After Continue: ENTER MSC → detect newly mounted volume (**`esp2/` not
required** — empty cards OK) → show destination metadata → **explicit
confirm** → restore + verify → leave MSC.

## Restore semantics (V1)

- Target: the ESP][ managed tree `/esp2/` on the selected volume
- Existing `/esp2` is **replaced** (staging + swap); files present only on the
  destination and not in the backup are removed from `/esp2`
- Unrelated files **outside** `/esp2` are left alone (no whole-drive wipe)
- Config/macros (`system.json`, `macros.json`) are restored as ordinary files —
  not regenerated

If restore fails mid-way, the destination may be incomplete; the tool reports
failure (never PASS).

## Verification

| Phase | Requirement |
| --- | --- |
| After backup | Every file exists; size and SHA-256 match source |
| Before restore | Manifest valid; backup hashes verified |
| After restore | Destination files re-read; size and SHA-256 match manifest |

## CLI (automation / tests)

```text
node dev/tools/esp2-sd-backup.mjs backup --source <mount> [--dest-root local/sd-backups]
node dev/tools/esp2-sd-backup.mjs restore --backup <dir> --dest <mount> [--dry-run]
node dev/tools/esp2-sd-backup.mjs list [--dest-root local/sd-backups]
node dev/tools/esp2-sd-backup.mjs verify --backup <dir>
```

µGulp remains the primary interactive UX.

## Safety

- Path traversal and absolute manifest paths rejected
- Symlinks / special files rejected on walk
- Restore writes only under the selected destination `esp2/` tree
- Source card is not modified by backup
- Destination drive is never guessed from “first removable” / size / letter

## Related

- Device config paths: `docs/architecture/device-config.md`
- Git vs NAS: `docs/tooling/backup.md`
- USB MSC ownership: Device → USB Storage Mode
