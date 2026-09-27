# CLAUDE.md — ESP][

## 1. Project Identity

Project name:

# ESP][

ESP][ is a miniature, self-contained Apple II emulator based on the
Waveshare ESP32-S3-Touch-AMOLED-1.64.

The project is a hobby/open-source project intended for publication
on GitHub.

The final physical device should resemble a miniature Apple II setup:

- Apple II computer
- Apple monitor
- one or more Disk II drives
- optional external paddles
- Bluetooth keyboard
- Bluetooth game controller
- Bluetooth headphones

The ESP32-S3 AMOLED board will be installed vertically inside the
miniature monitor.


## 2. Target Hardware

Primary board:

Waveshare ESP32-S3-Touch-AMOLED-1.64

Expected hardware:

- ESP32-S3
- 16 MB Flash
- 8 MB PSRAM
- 1.64" AMOLED
- 280 × 456 pixels
- CO5300 display controller
- FT3168 touch controller
- microSD
- Wi-Fi
- Bluetooth LE
- IMU if available on board
- USB-C

Additional planned hardware:

- passive piezo / local speaker output
- physical reset/control button
- Bluetooth keyboard
- Bluetooth gamepad
- Bluetooth headphones
- later: wired analog paddles


## 3. Development Environment

Use:

- PlatformIO
- ESP32-S3
- ESP-IDF where practical
- C/C++
- Git

Do NOT turn this into an Arduino IDE project.

Arduino components/libraries may be used where technically useful,
but PlatformIO remains the build and project environment.

Existing proven Waveshare board initialization code and known-working
PlatformIO ports should be reused where appropriate.

Do not rewrite working hardware initialization without a concrete
technical reason.


## 4. Development Strategy

Development must be incremental.

DO NOT attempt to implement the entire Apple II emulator in one step.

Every development task should:

1. inspect the existing implementation;
2. read this CLAUDE.md;
3. make the smallest reasonable change;
4. build with PlatformIO;
5. resolve compiler/linker errors;
6. upload to physical hardware when appropriate;
7. inspect the serial monitor;
8. diagnose runtime problems;
9. repeat until the milestone works;
10. preserve previously verified functionality.

Do not perform unrelated refactoring while debugging hardware.

Never replace known-working display, touch, SD, PSRAM or board
initialization merely because another implementation appears cleaner.


## 5. Agent-Based Hardware Debugging

Cursor/Grok is expected to perform as much of the development/debug
cycle itself as possible.

Typical cycle:

    pio run
    pio run -t upload
    pio device monitor

The agent should inspect serial output and iteratively correct problems.

Firmware diagnostics should therefore be structured and
machine-readable.

Example:

    [BOOT] ESP32-S3
    [FLASH] 16777216 bytes OK
    [PSRAM] 8388608 bytes OK
    [DISPLAY] CO5300 init OK
    [DISPLAY] 280x456
    [TOUCH] FT3168 OK
    [SD] mounted
    [BLE] initialized
    [MODE] LANDSCAPE_STANDALONE
    [SELFTEST] PASS

Failures should clearly identify subsystem and reason:

    [DISPLAY][FAIL] ...
    [SD][FAIL] ...
    [BLE][FAIL] ...

Successful API calls do NOT prove that visual display output is
correct.

Anything requiring visual confirmation must be shown to the user for
confirmation.


## 6. Milestone Rule

Every major milestone must leave behind:

- compiling project;
- flashable firmware;
- useful serial diagnostics;
- no regression of previously working hardware;
- clean Git state;
- preferably a Git commit/tag before the next risky milestone.

Do not continue through multiple major milestones when the current
milestone has not been verified.


# DISPLAY AND VIDEO


## 7. Physical Display Architecture

The physical AMOLED is 280 × 456.

The final enclosure will NOT expose the complete AMOLED area.

The board will be mounted vertically inside the miniature monitor.

In enclosure mode:

- the visible Apple II image begins below the rounded upper AMOLED
  corners;
- part of the lower AMOLED may disappear behind the monitor structure,
  drives and/or Apple II enclosure;
- the physical panel dimensions and visible viewport must therefore
  always be treated separately.

Never assume that the complete AMOLED is visible in enclosure mode.


## 7b. Display Power / Anti-Burn-In

Diagnostic and future UI firmware must manage AMOLED on-time.

Conceptual states (panel only — not ESP32 deep sleep):

- ACTIVE
- SCREENSAVER (mostly black + periodically relocating small mark)
- OFF (CO5300 display-off / sleep sequence)

Default diagnostic timeouts:

- screensaver after ~2 minutes inactivity
- panel off after ~5 minutes total inactivity

Wake sources call a central activity API (`notifyActivity`). The first
touch that wakes the panel is consumed and must not activate UI controls.
See `docs/architecture/display-power.md`.


## 8. Display Modes

Three primary display/UI modes are required.


### LANDSCAPE_STANDALONE

Used during development and when the board is operated without the
final enclosure.

The AMOLED is used horizontally.

The Apple II image should use as much of the available screen as
practical.


### PORTRAIT_APPLE2_CASE

Final miniature Apple II mode.

The physical board is vertical.

Only a defined rectangular viewport represents the visible monitor
screen.

The viewport must be configurable independently of physical panel
dimensions.


### CONTROL_SCREEN

Touch-enabled emulator control interface.

Opening the Control Screen should not require stopping the Apple II
emulation.

The Apple II may continue running in the background.

A small live preview may eventually be displayed.

Planned controls:

- game/library selection
- Drive 1
- Drive 2
- insert/eject disk
- write protection
- Bluetooth devices
- keyboard status
- gamepad status/configuration
- Bluetooth audio
- local audio
- volume
- video mode
- display effect
- display orientation
- reset Apple II
- restart ESP32
- pause/resume
- settings


## 9. Apple II Video Architecture

Keep Apple II video generation independent from the physical display.

Conceptually:

    Apple II video memory
            |
      Apple II video decoder
            |
       video signal/model
            |
       color renderer
            |
      display effects
            |
      viewport/scaling
            |
       physical AMOLED

Apple II HGR is 280 × 192.

Do NOT treat Apple II HGR as ordinary modern RGB bitmap graphics.

Authentic Apple II artifact-color behavior is a core requirement.


## 10. Video Color Modes

Video color generation and monitor effects are separate concepts.

Required color modes:

### COMPOSITE_COLOR

Authentic Apple II composite/artifact color rendering.

This is the primary mode for games.

The renderer must account for Apple II pixel phase/artifact behavior.

Do not simply map bits to arbitrary RGB colors.


### MONOCHROME_WHITE

True monochrome rendering.

Do NOT create this merely by desaturating the final composite image.

Render from the underlying Apple II video information so that
artifact-color fringes disappear.


### MONOCHROME_GREEN

Green phosphor monitor appearance.

Based on true monochrome video data, not desaturated composite RGB.


### MONOCHROME_AMBER

Amber phosphor monitor appearance.

Based on true monochrome video data, not desaturated composite RGB.


Suggested representation:

    enum class VideoColorMode {
        CompositeColor,
        MonochromeWhite,
        MonochromeGreen,
        MonochromeAmber
    };


## 11. Display / CRT Effects

Display effects are independent from VideoColorMode.

This allows combinations such as:

    CompositeColor + CRT_TV
    CompositeColor + Sharp
    MonochromeGreen + Monitor
    MonochromeAmber + Monitor
    MonochromeWhite + CRT_TV

Required conceptual effect modes:

### SHARP

No simulated CRT characteristics.

Useful for:

- debugging
- text
- maximum readability


### MONITOR

Simulates a good period monitor.

Possible characteristics:

- mild horizontal softness
- subtle scanlines
- phosphor persistence
- subtle bloom

Must remain readable on the small AMOLED.


### CRT_TV

Simulates viewing the Apple II through a period television/composite
display.

Possible characteristics:

- scanlines
- horizontal softness
- composite color bleeding
- slight luminance bleed
- subtle phosphor persistence
- subtle noise
- very subtle horizontal/vertical instability or jitter

Effects must remain subtle.

Do not destroy image readability merely to make the effect obvious.


## 12. Effect Strength

Effects should eventually support:

    OFF
    LOW
    MEDIUM
    HIGH

The default should favor readability.

Effects must be independently configurable from VideoColorMode.


## 13. Video Performance

ESP32-S3 resources are limited.

Do not assume desktop GPU shaders.

Prefer:

- lookup tables
- integer arithmetic
- line-based processing
- precomputed palettes
- minimal framebuffer copies

Avoid expensive full-frame transformations where possible.

The CO5300 display has limitations regarding hardware rotation.

Do not blindly rotate complete framebuffers when a more efficient
renderer/viewport transformation can be used.

Measure performance rather than guessing.


# INPUT


## 14. Bluetooth Keyboard

Bluetooth keyboard is a primary input device.

Architecture:

    Bluetooth keyboard
            |
         HID layer
            |
        +---+----------------+
        |                    |
    emulator             Apple II
     hotkeys              keyboard

Emulator hotkeys must be intercepted before they reach the emulated
Apple II.

Required emulator actions include:

- Control Screen
- switch display mode
- fullscreen Apple II
- switch video mode
- switch display effect
- disk controls
- pause/resume
- Apple II reset

Exact keyboard assignments must remain configurable.

Do not hard-code them deep inside the emulator core.


## 15. Bluetooth Gamepad

Primary reference controller:

8BitDo SN30 Pro

Other standard BLE HID gamepads should be supported where practical.

Initial mapping:

    left analog stick -> Apple II joystick X/Y
    A                 -> Apple button 0
    B                 -> Apple button 1

Additional buttons may control:

- Control Screen
- pause
- disk selection
- reset
- video mode

Gamepad handling must remain outside the Apple II core.


## 16. Wired Analog Paddles

Future hardware will support real analog Apple-style paddles.

Preferred connector:

3.5 mm TRRS.

One paddle may use:

    3.3 V
    GND
    analog position
    button

Potentially use two identical paddle connectors.

Firmware architecture should support:

    Bluetooth gamepad
    wired paddle
           |
    common Apple II
    joystick/paddle abstraction

Calibration should eventually support:

- minimum
- maximum
- center
- inversion
- deadzone


# AUDIO


## 17. Apple II Audio

Apple II speaker behavior must be emulated from the normal Apple II
speaker toggle mechanism.

Audio is a V1 requirement.

The speaker is a **cycle-accurate 1-bit state**. Every access to the
speaker soft-switch must produce an edge event stamped with the exact
emulated 6502 cycle.

**Permanent architecture rule:** Apple II speaker output must remain
represented as cycle-accurate 1-bit edge events until the final
audio-rendering (or BlueShift edge-transport) stage.

Do **not** reduce the signal prematurely to:

- notes
- tones
- frequencies
- fixed beeps
- frame-rate events

Historical software uses timed pulses / PWM for digitized audio. The PCM
renderer must **integrate** the 1-bit level over each output sample
interval so pulses shorter than one PCM sample still contribute. This
applies to local speaker, host audio, Bluetooth audio, and BlueShift.
BlueShift should preferably carry cycle-delta speaker edge events rather
than pre-rendered PCM when practical.


Conceptual architecture:

    Apple II speaker toggle
              |
         edge stream (cycle, 1-bit level)
              |
         audio engine / PCM integrator / BlueShift packer
              |
       +------+------+
       |             |
    local audio   Bluetooth audio


## 18. Local Audio

Support a passive piezo or other simple local transducer.

Do not assume an active fixed-frequency buzzer.

Local output is primarily intended as:

- simple built-in audio
- fallback
- debugging output


## 19. Bluetooth Audio

Bluetooth headphone/audio support is required in V1.

IMPORTANT:

ESP32-S3 Bluetooth limitations must be investigated experimentally.

Do NOT assume Bluetooth Classic or A2DP availability.

Determine what is technically possible with:

- ESP32-S3
- current ESP-IDF
- BLE Audio support
- target headphones

Bluetooth audio must be tested early.

Measure:

- latency
- dropouts
- CPU load
- memory consumption
- interaction with BLE keyboard
- interaction with BLE gamepad

Do not postpone Bluetooth audio until the end of the project.


# RESET


## 20. Reset Architecture

Two resets are required.


### APPLE II RESET

Resets only the emulated Apple II.

Mounted virtual disks remain inserted.


### ESP32 RESTART

Restarts the physical firmware.

Both should eventually be available from the Control Screen.

Apple II Reset should also have a Bluetooth keyboard shortcut.

A physical reset/control button is planned.


# DISKS AND STORAGE


## 21. Virtual Disk Architecture

Disk image handling must be separated from the Apple II core.

Conceptually:

    Apple II
       |
    Disk II card/controller emulation
       |
    Virtual Disk Controller
       |
       +---- Drive 1
       |
       +---- Drive 2
                |
            disk image
                |
             microSD

Do NOT implement only a high-level DOS filesystem abstraction.

Apple II software may directly access disk hardware and unusual disk
formats.


## 22. Disk Formats

Support may be introduced incrementally.

Architectural targets:

- DSK
- PO
- NIB
- WOZ

The architecture must not prevent later accurate support for
copy-protected or timing-sensitive disk images.


## 23. Virtual Disk Behavior

Required eventual operations:

- insert
- eject
- Drive 1 / Drive 2
- write protect
- writable disk
- swap disk
- remember mounted disks
- reset while keeping disks inserted

Disk activity should eventually be exposed to the UI.

Optional later enhancement:

- Disk II activity LED
- Disk II sound simulation


# GAME LIBRARY


## 24. User Experience

The user experience should conceptually resemble Apple ][js:

https://www.scullinsteel.com/apple2/

Users should normally select a title rather than manually browse raw
disk-image filenames.


## 25. Suggested SD Structure

Example:

    /apple2/
        Choplifter/
            game.json
            disk1.dsk
            cover.png

        NightMission/
            game.json
            disk1.dsk
            cover.png

        ExampleMultiDiskGame/
            game.json
            disk1.woz
            disk2.woz
            cover.png

game.json may eventually specify:

- title
- author/publisher
- year
- disk files
- boot disk
- default Drive 1
- default Drive 2
- joystick mode
- video mode
- display effect
- notes


## 26. Copyright

Do not embed copyrighted commercial Apple II disk images in the public
GitHub repository unless redistribution rights have been established.

Firmware and game media must remain separable.

Users may provide their own disk images.


# TOUCH


## 27. Touch

Touch is primarily intended for:

- Control Screen
- library navigation
- disk management
- settings

Apple II software must not depend on touch.

Touch handling belongs outside the Apple II emulation core.


# PHYSICAL DESIGN


## 28. Enclosure — high-level permanent rules

ESP][ is a **miniature Apple II–inspired** physical system (computer +
monitor + integrated Disk II bodies), not a 1:1 replica of the original
enclosure.

**Authoritative detailed mechanical specification:**

    3dprint/ENCLOSURE-SPEC.md

Machine-readable companions:

    3dprint/dimensions/*.json
    3dprint/BOM.md
    3dprint/WIRING.md
    3dprint/SOURCES.md

Do **not** duplicate the full mechanical specification here. If this section
and `ENCLOSURE-SPEC.md` ever disagree, **`ENCLOSURE-SPEC.md` wins** for
mechanical design.

Permanent high-level rules:

- Beige upper = one coherent visible enclosure (computer + drives + monitor).
- Gray = separate screwed bottom plate (service access).
- Brown = one-piece keyboard/key structure inserted from below.
- Black = separate drive-front inserts.
- White = keyboard legends + badge carriers (no copyrighted logo art in CAD).
- White/translucent = recessed POWER indicator (not a switch).
- Waveshare board inserts **from below** into the upper enclosure; guides/stops
  align the AMOLED; screws only retain.
- Three display geometries remain distinct: physical glass, active area,
  ESP][ visible monitor window (see ENCLOSURE-SPEC / `display-geometry.json`).
- OLED upper roundings must **not** be visible in the finished monitor opening.
- Chassis rear USB-C is the external port; Waveshare USB-C stays **internal**
  (90° adapter + short extension + panel socket). Do **not** expose the module
  USB-C as the primary user port.
- microSD may remain internal (no required external SD door).
- Protect the LiPo: no sharp printed pressure against the pouch.
- Prefer parameterized B-Rep → STEP (+ Parasolid when tooling permits) →
  Plasticity. Do not treat STL mesh editing as the primary CAD workflow.


## 29. Power Concept (high-level)

Internal LiPo + Waveshare charge path; real ON/OFF is a **rear** mechanical
switch. Front POWER is an illuminated indicator only.

External power/data enter through the **chassis** USB-C panel socket (not by
plugging into the Waveshare connector on the side of the module).

Exact battery capacity, switch P/N, and USB panel geometry remain
`TO_BE_SELECTED` / `PENDING_COMPONENT_SELECTION` in `3dprint/BOM.md`.


## 30. Accessories (paddles / joystick) — high-level

Optional accessories are **1:1 original Apple II size**, not miniature
enclosure scale:

- Paddles: `3dprint/paddles/`
- Joystick: `3dprint/joystick/`

The project author owns **physical original paddles and an original joystick**
for final dimensional validation. Modified paddle buttons on those physical
units are **not** authoritative for original button geometry.

Detailed FIXED / PROVISIONAL / UNKNOWN rules live in the accessory specs —
not here.


# SOFTWARE ARCHITECTURE


## 31. Module Separation

Keep these concerns separate:

    hardware/
    display/
    video/
    input/
    bluetooth/
    audio/
    media/
    ui/
    apple2/

The Apple II core must not know whether input originated from:

- Bluetooth keyboard
- Bluetooth gamepad
- wired paddle
- touch
- future input device

The Apple II core must not know whether its output is displayed:

- horizontally
- vertically
- inside the enclosure viewport
- as a Control Screen preview


## 32. Performance Diagnostics

Debug builds should eventually expose:

- Apple II emulation speed
- effective CPU frequency
- FPS
- frame time
- render time
- free internal RAM
- free PSRAM
- audio buffer state
- audio underruns
- Bluetooth status
- disk latency

Do not optimize blindly.

Measure first.


# OPEN SOURCE


## 33. Existing Implementations

Before implementing major Apple II components from scratch, investigate
existing open-source implementations.

Potential reusable areas:

- 6502 CPU
- Apple II memory map
- Apple II ROM handling
- text mode
- LoRes
- HiRes
- artifact colors
- Disk II
- NIB
- WOZ

Check licenses before copying or adapting code.

Document source and license of incorporated third-party code.

Do not blindly port an entire desktop emulator if individual
components are more suitable.


# FIRST DEVELOPMENT PHASE


## 34. Phase 1 — Hardware Bring-Up ONLY

The first development phase is hardware bring-up.

DO NOT implement Apple II emulation yet.

Required sequence:

1. PlatformIO project builds.
2. Firmware uploads.
3. Serial monitor works.
4. ESP32-S3 detected correctly.
5. Flash size verified.
6. PSRAM verified.
7. AMOLED initialized.
8. Known test pattern displayed.
9. Landscape rendering verified.
10. Portrait rendering verified.
11. Visible viewport concept tested.
12. Touch verified.
13. microSD verified.
14. basic hardware self-test reports results.

A useful test pattern should clearly identify:

- TOP
- BOTTOM
- LEFT
- RIGHT
- orientation
- resolution
- colors

The user must visually confirm display orientation and correctness.

Only after Phase 1 is verified should Apple II emulator development
begin.


# AGENT RULES


## 35. Rules for Cursor / Grok / Other Agents

Always read this CLAUDE.md before modifying the project.

Do not interpret a future implementation prompt as permission to
discard previously verified functionality.

When working on a milestone:

- stay within milestone scope;
- do not implement future milestones opportunistically;
- do not perform unrelated refactoring;
- preserve known-working board code;
- build after changes;
- upload when appropriate;
- inspect serial output;
- fix problems iteratively;
- report what was actually verified.

Do not claim physical hardware success when it has only compiled.

Do not claim visual success without user confirmation.

If an architectural decision in this document appears technically
impossible, investigate it and report the evidence before replacing
the decision.

When uncertain:

**preserve the working implementation and investigate first.**

# ENGINEERING STANDARDS


## Code Style

Use a consistent C/C++ style throughout ESP][.

General rules:

- Use modern C++ where supported by the selected ESP32 toolchain.
- Prefer explicit, readable C++ over clever constructs.
- 4 spaces indentation.
- Never use tabs for indentation.
- Opening braces remain on the same line.
- One statement per line.
- Keep functions focused and reasonably small.
- Prefer early returns where they improve readability.
- Avoid deeply nested control flow.
- Avoid mutable global state where practical.
- Do not perform unrelated refactoring during hardware/debug milestones.

Naming:

    Types / classes:      PascalCase
    functions / methods:  camelCase
    local variables:      camelCase
    members:              camelCase
    constants:            kPascalCase
    enum classes:         PascalCase
    enum values:          PascalCase

Example:

    enum class VideoColorMode {
        CompositeColor,
        MonochromeWhite,
        MonochromeGreen,
        MonochromeAmber
    };

Keep naming consistent across modules.


## Automatic Formatting

Use clang-format.

Maintain:

    .clang-format

Formatting must be deterministic.

Agents must format modified project-owned C/C++ files before completing a
task when clang-format is available.

Do NOT reformat unrelated files during feature/debug work.

Do NOT format vendored/third-party source unless that source is intentionally
maintained as part of ESP][.

Formatting-only changes should preferably remain separate from functional
changes.


## Editor Configuration

Maintain:

    .editorconfig

Use:

- UTF-8
- LF where practical
- final newline
- 4 spaces for C/C++
- no trailing whitespace

Do not introduce conflicting editor-specific formatting rules.


## Static Analysis / Warnings

Where practical use:

- compiler warnings
- PlatformIO check
- clang-tidy where useful

Project-owned code should compile without avoidable warnings.

Do not globally suppress warnings merely to hide local problems.

Third-party warnings must not drown project-owned diagnostics.


# I18X / INTERNATIONALIZATION


## i18x Is Required

ESP][ user-facing text must be internationalizable from the beginning.

Do NOT scatter hard-coded user-facing strings throughout firmware.

This includes:

- Control Screen
- virtual keyboard UI
- disk/library UI
- Bluetooth UI
- diagnostics shown to the user
- errors
- warnings
- pairing instructions
- video/audio settings
- calibration screens
- battery/power messages
- help text


## Existing i18x Concept

Use the existing project-family i18x concepts where practical.

Do NOT create a second incompatible localization system merely because a
simple translation table would be easier initially.

i18x is broader than simple string translation.

The architecture must allow locale-specific handling of:

- text
- numbers
- percentages
- durations
- dates/times where applicable
- units
- pluralization where required
- other locale-dependent formatting


## Initial Locales

At minimum prepare for:

    en-US
    de-DE

English is the technical fallback locale.

Additional locales must be addable without changing application logic.


## Firmware Resource Constraints

ESP][ is embedded software.

Localization must be flash/RAM conscious.

Prefer:

- immutable resources in flash
- compact lookup structures
- lazy/on-demand access where useful

Avoid loading all translations into RAM.

Do not allocate translated strings repeatedly inside hot rendering loops.


## String Keys

Application code should refer to stable semantic keys rather than literal
English text.

Conceptually:

    i18x("control.reset")
    i18x("disk.insert")
    i18x("bluetooth.pair")
    i18x("diagnostic.ok")

Exact API may follow the existing i18x implementation.

Do not use English source text itself as the permanent translation key.


## OLED / Small Display Constraints

Translations must not assume English string length.

The 280x456 display is small and the final enclosure exposes an even smaller
viewport.

UI layouts must tolerate:

- longer translations
- abbreviations
- clipping where unavoidable
- scrolling where useful
- responsive sizing

Do not hard-code widget widths around one English phrase.


## Diagnostics and Machine Logs

Human-facing diagnostic text should use i18x where appropriate.

Machine-readable logs MUST remain language-independent.

For example:

    [DIAG] test=imu step=tilt_left result=ok

not:

    [DIAG] test=imu step=nach_links_neigen result=ok

Diagnostic JSON field names and enum values should remain stable and
language-neutral.

This allows Cursor/agents/tools to analyze logs independent of the selected
UI language.


## Game Metadata

Game/library metadata must distinguish machine-readable values from localized
display text.

Future game metadata should allow localized fields where useful without
requiring duplicate disk definitions.

Do not design game.json around German-only or English-only text.


# DOCUMENTATION INTERNATIONALIZATION


## Documentation Sources

Follow the established µGulp documentation workflow.

Locale sources (edit these — first-class, independently reviewable):

    dev/docs/readme/en-US.src.md
    dev/docs/readme/de-DE.src.md

Generated GitHub READMEs (do not edit by hand):

    README.md          ← en-US (GitHub default)
    README.de-DE.md    ← de-DE

Filtered baselines may also be written under `dev/docs/readme/*.md`.

Future README changes must be made in the locale `.src.md` files / structured
data, not in the generated README files.

`npx gulp docs` regenerates both. Optional: `docs:en-US`, `docs:de-DE`.


## Generated Files

Generated documentation must be deterministic.

Running generation twice without source changes must not create meaningless
Git differences.

Do not manually edit generated README content when the source belongs in
`en-US.src.md` or `de-DE.src.md`.

The canonical µGulp-ready badge asset is:

    docs/assets/microgulp-ready.png

Do not invent alternate badge artworks or language-specific copies.


# ARCHITECTURAL STRING RULE


## Core Logic Must Not Depend on Display Language

The following layers must remain language-independent:

    apple2/
    media/
    video/
    input/
    audio/
    bluetooth/
    diagnostics core

Localized strings belong at presentation boundaries.

For example:

    DiskError::ImageInvalid

is core state.

The UI may render it as:

    de-DE: "Ungültiges Diskettenabbild"
    en-US: "Invalid disk image"

Never make program logic depend on comparing translated text.


# FORMAT / I18X TESTING


## Formatting Validation

Where tooling is available, provide:

    format
    format:check

through the project's Gulp/µGulp workflow.

A formatting check must not modify source.


## i18x Validation

Provide automated checks where practical for:

- duplicate keys
- missing fallback strings
- malformed locale resources
- unknown locale IDs
- invalid formatting definitions

Missing translations may fall back to en-US during development but should be
reported.


## Future Translation Workflow

Prepare i18x resources so that future automated translation/generation can
operate on source locale resources without modifying application code.

Do not bake generated translations into C++ manually.


# AGENT RULES FOR STYLE AND I18X


## Cursor / Grok / Other Agents

Before creating new UI strings or C/C++ modules:

1. read CLAUDE.md;
2. follow project naming/formatting rules;
3. use the established i18x layer for user-facing strings;
4. keep machine-readable diagnostics language-neutral;
5. format modified project-owned source;
6. avoid unrelated formatting changes.

Agents must NOT:

- introduce a second localization framework;
- hard-code large sets of English UI strings;
- translate protocol names or stable machine identifiers;
- reformat the entire repository during an unrelated task;
- modify vendored code solely to match ESP][ style.


# APPLE II COMPATIBILITY AND PORTABILITY


## No title-specific emulator hacks

Never special-case commercial titles inside the Apple II core
(e.g. `if title == "Choplifter"`). Fix machine, timing, device, or parser
behavior with generic regression tests.


## Host vs ESP32 compatibility

`hostStatus` and `esp32Status` are independent evidence fields.
A host result must never be reported as ESP32 compatibility.


## Emulated cycle timeline

The 6502 cycle counter is the authoritative Apple II timeline for speaker,
paddles, Disk II, and related devices. FreeRTOS / wall-clock time is only for
real-time throttling and UI — not emulated machine time.


## Video memory and dirty tracking

- Apple II logical **HGR remains 280 × 192** permanently.
- Every emulated VRAM write (and its cycle cost) remains fully executed.
- Dirty metadata may optimize physical display transfers only (scanline/row
  bitsets, coalescing, frame skip).
- Physical display may drop or coalesce frames when behind.
- Apple II emulated cycles must **never** be dropped to chase display FPS.


## Real-software assets

Real Apple ROM / Disk II ROM / commercial disk testing uses
**USER_SUPPLIED_ONLY** local assets unless redistribution rights are
established. CI must stay green without proprietary media.
Compatibility claims require recorded evidence (see
`docs/apple2/compatibility-testing.md`).


## Backup — Git vs NAS (independent)

**Permanent rule:** Git eligibility and NAS backup eligibility are independent.

- Local / downloaded / user-supplied assets that are valuable to reproduce the
  project (**SHOULD** be privately NAS-backed) even when they must **never**
  enter Git (`.gitignore` / `GIT_BACKUP_NEVER_STAGE`).
- Examples: `local/apple2/` (ROMs, disks, SST vectors, manifests),
  `local/roms/`, `_refs/`, gitignored `3dprint` vendor dumps.
- Disposable caches (`node_modules`, `.pio`, …) stay out of NAS backup.
- Details: `docs/tooling/backup.md`.
