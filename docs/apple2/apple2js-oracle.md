# Using apple2js as a development oracle

apple2js is a convenient browser reference for Apple II behavior. ESP][ must
not copy its implementation blindly; compare outcomes.

## Useful comparison areas

| Area | How to compare |
| --- | --- |
| Disk boot | Same user-supplied DSK/PO in apple2js drag-drop vs ESP][ host harness |
| HGR artifact color | Screenshot / pixel samples of known synthetic patterns |
| Joystick | Same axis mapping expectations for paddle/button soft-switches |
| Soft switches | Text/mixed/page2 visibility |
| Speaker | Toggle frequency vs audible click rate (qualitative) |

## Workflow

1. Keep a **user-owned** disk image outside the ESP][ git tree.
2. Load it in apple2js (website or local `npm start` from `.cache/apple2js`).
3. Run the same scenario on the ESP][ host harness / later firmware.
4. Record differences in issue notes with language-neutral diagnostics.

## Do not

- Commit website JSON disk blobs.
- Treat website availability as a license.
- Port GPL or large browser stacks into firmware.
