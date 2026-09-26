# Host → Apple II key map

Normalized host keys enter through `AppleIIKeyMap` then `Keyboard::keyDown`.
Emulator hotkeys must be intercepted **before** this path.

| Host | Apple II (7-bit) |
| --- | --- |
| A–Z / a–z | same ASCII (as typed) |
| 0–9, punctuation | same ASCII |
| Space | `$20` |
| Return / Enter | `$0D` |
| Escape | `$1B` |
| Backspace / Delete | `$08` (left arrow) |
| Ctrl+A … Ctrl+Z | `$01` … `$1A` |

## Emulator hotkeys (host runner)

Not injected into Apple II:

- `quit`, `reset`, `text`, `diag`, `run`, `type`
