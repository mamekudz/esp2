# Disk formats (interfaces)

## Targets

| Format | Status this milestone |
| --- | --- |
| DSK | Host read-only parse + synthetic fixtures |
| PO | Host read-only parse (same 143360 layout family) |
| NIB | Research only |
| WOZ | Research only |

## Layout notes (DSK/PO)

Common 35-track × 16-sector × 256-byte images → **143360** bytes.

- DSK: DOS order
- PO: ProDOS order  

Host code must bounds-check track/sector and reject truncated files.

## Virtual drive state

Two drives; each tracks inserted/ejected, image id, write protect, dirty,
activity. Apple II reset does **not** eject.
