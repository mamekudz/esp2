# ESP][ host runner

Console host for development (no large GUI).

## Build

```bash
npm run test:apple2
# produces host/.out/esp2_host.exe
```

## Usage

```text
esp2_host [--rom path] [--machine AppleII|AppleIIPlus]
          [--slot6 none|synthetic] [--disk1 Esp2DiskTest]
          [--cycles N] [--text] [--diagnostics] [--batch]
          [--video-dump out.ppm]
```

Without `--rom`, the project **synthetic** motherboard ROM is used (always legal).

### Emulator commands (not Apple keys)

| Command | Action |
| --- | --- |
| `quit` | Exit |
| `reset` | Apple II reset |
| `text` | Dump logical 24×40 text |
| `diag` | Status overlay |
| `run N` | Run N cycles |
| `type …` | Inject string via key map |

Other lines: first character → Apple II keyboard latch.

## Keyboard path

```
host key → AppleIIKeyMap → Keyboard::keyDown → $C000 / strobe $C010
```

See `docs/apple2/key-map.md`.

## Gulp

```bash
gulp apple2:host --rom path/to.rom
gulp apple2:rom-test --rom path/to.rom
gulp apple2:rom-identify --rom path/to.rom
```
