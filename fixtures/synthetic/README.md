# Synthetic fixtures

Only project-generated data. **No** commercial Apple II media or ROMs.

- `library/DemoCart/game.json` — metadata schema sample
- `library/DemoCart/disk1.dsk.bin` — blank 143360-byte image (generated)
- `apple2js/sample-index.json` — tiny fake catalog for host tests
- `apple2js/synthetic-zero.dsk.json` — legal zero-filled JSON disk for converter tests

Generate / refresh the blank disk:

```
node host/tools/gen_blank_dsk.mjs
```

Refresh the synthetic JSON disk:

```
node fixtures/synthetic/apple2js/gen_synthetic_json_disk.mjs
```
