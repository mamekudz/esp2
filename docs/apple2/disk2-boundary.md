# Disk II controller boundary (NEXT milestone)

Status: **BOUNDARY ONLY** — not implemented in the host-machine milestone.

## Slot integration

- Typical Disk II card in **Slot 6**.
- Soft switches in `$C0x0–$C0xF` window for slot `x` (slot 6 → `$C0E0–$C0EF`).
- Card ROM often `$C600–$C6FF` (slot 6 PROM) + optional expansion.

ESP][ must keep Disk II behind a controller abstraction — the CPU bus
dispatches I/O; the core must not open files.

## ROM requirements

- Card PROM (user-supplied or synthetic stub) — **never** commit Apple firmware.
- Machine language boot relies on PROM + DOS image content.

## Timing / cycle requirements

- Motor spin-up delay (~1 s real time).
- Nibble read rate tied to disk rotation (~4 µs/bit class timing).
- WOZ / NIB need finer than instruction-level CPU for copy protection.
- Current fake6502 is **instruction-level** — Disk II accuracy may need
  cycle hooks or a dedicated disk timing co-model later.

## Drive state (conceptual)

| State | Notes |
| --- | --- |
| Motor on/off | Soft switch |
| Phase magnets 0–3 | Stepper head |
| Track position | Quarter-tracks for WOZ |
| Shift / data latch | Read/write nibble path |
| Write protect | From image / virtual drive |

## Image formats

| Format | Controller needs |
| --- | --- |
| DSK/PO | Sector rebuild → nibble stream (or high-level路径 for early bring-up) |
| NIB | Raw track nibbles |
| WOZ | Flux/nibble + timing + metadata |

Early ESP][ path: DSK/PO via VirtualDrive; NIB/WOZ after research
(`docs/apple2/nib-woz-research.md`).

## What NOT to do

- Do not implement a fake “load DOS file” API inside the CPU bus.
- Do not block the 6502 step loop on FAT/SD I/O — use bounded queues.
- Do not assume one title == one file (multi-disk already in media tooling).
