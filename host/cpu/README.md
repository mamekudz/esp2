# 6502 host harness (skeleton)

No third-party CPU core is vendored yet (`docs/apple2/core-evaluation.md`).

Planned host tests (after permissive core selection):

- reset vector fetch
- LDA/STA
- branches
- stack PHA/PLA
- JSR/RTS
- flags
- optional cycle counts

Use a **synthetic** stub ROM in `fixtures/synthetic/roms/` only — never Apple ROMs.

Until a core is selected, this directory holds the plan only.
