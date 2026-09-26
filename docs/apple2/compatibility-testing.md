# Compatibility testing (host)

ESP][ real-software testing uses **USER_SUPPLIED_ONLY** media and ROMs unless
redistribution rights are established. CI never downloads proprietary assets.

## Status model

| Status | Meaning |
| --- | --- |
| NOT_TESTED | No run yet |
| BOOTS | Reached post-boot code |
| TITLE_SCREEN | Title/start observed |
| INTERACTIVE | Accepts input |
| PLAYABLE | Sustained interactive play |
| COMPLETED_TEST_PATH | Automated checkpoints passed |
| PARTIAL | Some stages only |
| FAILS | Run failed |
| BLOCKED_MISSING_ASSET | Legal local asset absent |

**hostStatus ≠ esp32Status.** A host pass does not imply ESP32 playability.

Asset gate statuses: `SKIPPED_NO_SYSTEM_ROM`, `SKIPPED_NO_SLOT6_ROM`,
`SKIPPED_NO_MEDIA`, `UNSUPPORTED_*`, `TIMEOUT` — missing assets are **not** CI failures.

## Harness

```bash
node host/tools/compat_runner.mjs --list
node host/tools/compat_runner.mjs --test choplifter
gulp apple2:compat --test esp2-boot-test --run
```

Config: `config/apple2.local.example.json` → copy to gitignored `config/apple2.local.json`.

Test definitions live in `compatibility/tests/*.json` (no media bytes).

Matrix: `docs/compatibility/titles.json`.

## Permanent rules

- No title-specific emulator core hacks.
- Compatibility fixes must be generic hardware/timing/device corrections with
  regression tests.
- Do not invent SHA-256 for commercial media.
- Star Blazer browser audio is `SUBJECTIVE_REFERENCE` only.

## Level 5

Preparation complete when the harness is READY. Level 5 itself is **PASS** only
after a real user-supplied Apple II/II+ configuration boots interactively without
title-specific hacks. Until then:

`LEVEL 5 = READY_FOR_REAL_SOFTWARE_TEST`
