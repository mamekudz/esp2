# Backup policy — Git vs NAS

**Permanent rule:** Git eligibility and NAS backup eligibility are
**independent**.

| Property | Meaning |
| --- | --- |
| `GIT_TRACKED` | May enter the public repository |
| `NAS_BACKED_UP` | Copied to the user’s private NAS destinations |
| `REDISTRIBUTABLE` | Legal right to redistribute (orthogonal to backup) |

## Examples

| Asset | GIT_TRACKED | NAS_BACKED_UP | REDISTRIBUTABLE |
| --- | --- | --- | --- |
| Project source (`src/`, `docs/`, …) | YES | YES | YES (project license) |
| `local/apple2/` (AppleIIGo, disks, SST, manifests) | NO | **YES** | per file |
| `local/sd-backups/` (logical ESP][ microSD backups) | NO | **YES** | per file (often NO) |
| User Apple II+ `local/roms/*.rom` | NO | **YES** | NO (USER_SUPPLIED_ONLY) |
| `_refs/` / `3dprint` vendor dumps | NO (gitignored payloads) | **YES** | usually NO |
| `node_modules/`, `.pio/` | NO | NO | n/a |

Do **not** use `.gitignore` as the NAS exclusion list.

## Git

- `local/apple2`, `local/roms`, `local/sd-backups`, `library/user` remain
  **gitignored** and listed in `GIT_BACKUP_NEVER_STAGE`
  (`dev/tools/git-backup.mjs`).
- Downloads abort if the destination is not gitignored.

## NAS (`gulp backup` / `backup:nas` / `backup:all`)

Include (among others):

- Tracked trees: `src`, `include`, `docs`, `dev`, `config`, `host`, …
- Local assets: `local/apple2`, `local/roms`, `local/sd-backups`, `library/user`
- References: `_refs`, `3dprint` (includes gitignored vendor STEP/PDF/ZIP)

Exclude disposable caches by basename: `node_modules`, `.pio`, `.cache`,
`.microgulp`, `tmp`, `temp`, `.out`, `coverage`, …

Private NAS `BACKUP_MANIFEST.json` lists **categories only** (no proprietary
payload inventory for public docs).

Logical SD backups under `local/sd-backups/` are valuable disaster-recovery
assets; finishing an SD backup does **not** auto-start NAS — the next normal
NAS run includes them. Details: `docs/architecture/sd-backup-restore.md`.

## Restore

`RestoreLocalAssetsFromBackup` in `dev/tools/nas-backup.mjs` restores local /
reference trees from a NAS mirror. Restored files must remain gitignored —
`git status` must not suddenly show them as trackable.

## Configure destinations

See README / `config/nas.targets.example`. Up to three paths via form,
`NAS_TARGET_1..3`, or gitignored `config/nas.targets.local`.
