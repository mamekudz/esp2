# NIB / WOZ research (no half-parser)

## NIB

- Typically raw nibble streams approximating Disk II bit cells.
- Easier than WOZ but weaker for copy-protection fidelity.
- Memory: can stream from SD track-by-track.

## WOZ (preferred long-term fidelity)

Authoritative documentation should be taken from the WOZ specification
maintained by the Apple II community (Patterson / Applesauce lineage).  
**Action:** fetch and cite the current WOZ 2.x spec URL when implementing;
do not invent chunk layouts from memory.

### Implications for ESP][

| Topic | Implication |
| --- | --- |
| Timing | Flux/bit timing matters; may need cycle-accurate Disk II later |
| RAM | Do not keep entire multi-disk WOZ sets in SRAM |
| Streaming | Prefer SD seek + track cache |
| Complexity | Delay implementation until DSK/PO path is solid |

## Policy

Documented research only in this milestone. No incomplete WOZ parser.
