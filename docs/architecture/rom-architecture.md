# ROM architecture

## Policy

- Repository and firmware remain usable **without** Apple ROMs committed.
- Users supply ROMs they have rights to use.
- Never download or vendor copyrighted Apple ROMs in CI or Git.

## Conceptual layout (SD)

```
/apple2/roms/
  apple2plus.rom          # user-provided example name
  checksums.json          # optional user manifest
/apple2/roms/test/
  synthetic_reset.rom     # project-generated test stub (non-Apple)
```

## Loader API (host)

See `docs/apple2/roms.md`. Host loader validates size (12 KiB), computes SHA-256,
looks up metadata-only DB (`host/data/rom_database.json`), and loads into `Rom`.

Unknown hashes remain loadable (user-supplied); profile may be selected explicitly.

## Validation

- Reject unexpected sizes for known kinds (document expected sizes).
- Test stub ROMs may be any small vector with a valid reset vector for host
  CPU tests.
- Fail soft: device can still run diagnostics without ROM.

## Legal

Incorporating ROM *loaders* is fine. Incorporating ROM *bytes* requires a
clear redistribution right. Prefer synthetic stubs in fixtures.
