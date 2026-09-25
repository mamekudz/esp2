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

## Loader API (planned)

```
enum class RomKind { AutostartMonitor, Applesoft, IntegerBasic, TestStub };

struct RomImage {
  RomKind kind;
  uint32_t size;
  uint32_t crc32;   // optional
  const uint8_t* bytes; // mapped from flash/SD/PSRAM
};

RomLoadResult loadRom(StorageBackend&, const char* path, RomImage* out);
```

## Validation

- Reject unexpected sizes for known kinds (document expected sizes).
- Test stub ROMs may be any small vector with a valid reset vector for host
  CPU tests.
- Fail soft: device can still run diagnostics without ROM.

## Legal

Incorporating ROM *loaders* is fine. Incorporating ROM *bytes* requires a
clear redistribution right. Prefer synthetic stubs in fixtures.
