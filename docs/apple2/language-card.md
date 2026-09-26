# Language Card boundary (Apple II / II+)

Status: **DEFERRED** for ESP][ V1 floppy-first path.

## Role

The Language Card (typically **Slot 0**) banks RAM into `$D000–$FFFF`,
allowing Integer/Applesoft bank switching and larger programs. Soft switches
live in `$C080–$C08F` (slot-0 I/O window).

## Switch family (II+ / Language Card)

Exact banking differs by card revision; conceptually:

| Access | Typical effect |
| --- | --- |
| `$C080` / `$C084` | Read RAM, write-protect / bank select variants |
| `$C081` / `$C085` | Read ROM, write RAM enable (often needs **two** accesses) |
| `$C082` / `$C086` | Read ROM, write-protect |
| `$C083` / `$C087` | Read/write RAM bank 2 |
| `$C088–$C08F` | Related bank1 / inhibit combinations |

**Write-enable quirk:** many cards require **two consecutive** accesses to the
write-enable switch before RAM writes are accepted. Do **not** model this as
ordinary flat RAM.

## ESP][ decision

| Item | Status |
| --- | --- |
| Slot-0 I/O routing via `SlotDevice` | HOST_VERIFIED (empty / spy) |
| Full Language Card banking | **DEFERRED** |
| Needed for synthetic test ROM | No |
| Needed for many Disk II games | Often no (48K enough) |
| Needed for some DOS / large titles | Later |

Boot-readiness marks `language_card: DEFERRED`. Do not claim PASS.

## Future integration

Implement as a dedicated `LanguageCardDevice : SlotDevice` with explicit
bank state — never by making `$D000–$FFFF` silently writable RAM.
