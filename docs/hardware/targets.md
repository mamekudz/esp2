# ESP][ hardware targets

Target selection is owned by the µGulp **Project Control** (`platform`), not by
per-task board dialogs.

| Platform ID | UI label | Status | Default PlatformIO env |
| --- | --- | --- | --- |
| `waveshare-amoled-s3` | Waveshare AMOLED | Production ESP][ | `apple2_text` |
| `diymore-es3c28p` | diymore 2.8" | Bring-up / in development | `diymore_bringup` |
| `waveshare-rlcd42` | RLCD 4.2 | Bring-up / in development | `rlcd42_bringup` |

Part number for diymore (metadata only): `AM03-FBBA0125-AAA`.

RLCD 4.2: Waveshare/Luckfox ESP32-S3-RLCD-4.2 — see `docs/hardware/waveshare-rlcd42.md`.

## gulp-mu-build-filter

Source regions may use exact platform ids:

```c
/*-- @<BUILD_ONLY_AT_PLATFORMS:waveshare-amoled-s3 --*/
/* ... */
/*-- @>BUILD_ONLY_AT_PLATFORMS --*/
```

Pass `{ platform: '<id>' }` from `dev/tools/esp2-platforms.mjs` →
`buildFilterPlatformOptions()`.

## Persistence

Chosen target is stored by µGulp under `.microgulp/project-controls.json`
(gitignored with `.microgulp/`).
