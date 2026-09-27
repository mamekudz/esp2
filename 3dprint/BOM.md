# ESP][ Bill of Materials (mechanical + electrical)

Status vocabulary: `SELECTED` `ORDERED` `RECEIVED` `PHYSICALLY_VERIFIED` `CANDIDATE` `TO_BE_SELECTED`

CAD dependency: `CRITICAL` `REFERENCE` `NONE`

Do **not** freeze CAD openings for `CRITICAL` parts that remain `TO_BE_SELECTED`.  
Do **not** invent shopping URLs merely to fill blanks.

---

## MAIN ESP][

| ID | Component | Function | Qty | Exact model/spec | Mechanical dimensions | Electrical requirements | Mounting method | Color | Status | CAD dependency | Supplier | Product URL | Notes |
| --- | --- | ---: | ---: | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| MAIN-01 | Waveshare ESP32-S3-Touch-AMOLED-1.64 | Compute + AMOLED + touch + SD + IMU | 1 | ESP32-S3-Touch-AMOLED-1.64 (Rev1.1 observed on official drawing) | See `dimensions/board.json` (OFFICIAL) | 5 V USB / LiPo via MX1.25; USB CDC | Guides + stops + M2 screws to upper enclosure | — | PHYSICALLY_VERIFIED (dev board in use) | CRITICAL | Waveshare | https://www.waveshare.com/esp32-s3-touch-amoled-1.64.htm | Authoritative mechanical package: DAD.zip |
| BAT-01 | LiPo battery | Portable power | 1 | TO_BE_SELECTED (3.7 V pouch, MX1.25 compatible) | PENDING_COMPONENT_SELECTION | Matches Waveshare charge circuit; capacity TBD | Soft retention, no sharp bosses | — | TO_BE_SELECTED | CRITICAL | — | — | Protected compartment; service access |
| AUD-01 | Internal piezo / small speaker | Local Apple II speaker output | 1 | TO_BE_SELECTED (passive / simple transducer — not fixed-tone buzzer) | PENDING_COMPONENT_SELECTION | Drive via amplifier or safe GPIO PWM path — **not** headphones | Mount near ventilation slots | — | TO_BE_SELECTED | REFERENCE | — | — | Acoustic path via Apple II–style slots |
| AUD-02 | 2.5 mm TRRS panel socket | Rear accessory / audio concept | 1 | 2.5 mm 4-pole TRRS — exact P/N pending | PENDING_COMPONENT_SELECTION | Pinout undecided | Panel mount rear | — | TO_BE_SELECTED | CRITICAL | — | — | Do not invent hole diameter; may serve paddles and/or digital accessory link |
| PWR-01 | Rear ON/OFF switch | Real power control | 1 | TO_BE_SELECTED | PENDING_COMPONENT_SELECTION | Series battery/load path TBD | Panel mount rear | — | TO_BE_SELECTED | CRITICAL | — | — | Front POWER is indicator only |
| USB-01 | USB-C 90° adapter | Redirect Waveshare USB-C | 1 | USB 2.0 data capable | PENDING_COMPONENT_SELECTION | USB 2.0 D+/D− + power (+ CC as required) | Mate to Waveshare USB-C | — | TO_BE_SELECTED | CRITICAL | — | — | Must not stress PCB connector; **charge-only adapter forbidden** |
| USB-02 | Short USB-C data extension | Board → panel | 1 | USB 2.0 data | Length TBD | USB 2.0 D+/D− + power (+ CC as required) | Routed inside cavity | — | TO_BE_SELECTED | CRITICAL | — | — | Keep short; strain-relieved; **charge-only cable forbidden** |
| USB-03 | USB-C panel socket (female) | Chassis USB | 1 | Panel-mount USB-C, data+power | PENDING_COMPONENT_SELECTION | USB 2.0 + power + flash + CDC (+ future MSC); future Host/OTG **unvalidated** | Rear panel; absorbs plug force | — | TO_BE_SELECTED | CRITICAL | — | — | Preserve data+CC for role options; see `docs/architecture/input-providers.md` |
| LED-D1 | Red mini LED | Drive 1 activity | 1 | TO_BE_SELECTED | PENDING_COMPONENT_SELECTION | ~2 V fwd; series RES-D1 | In black Drive 1 front | red | TO_BE_SELECTED | REFERENCE | — | — | Software-driven later |
| LED-D2 | Red mini LED | Drive 2 activity | 1 | TO_BE_SELECTED | PENDING_COMPONENT_SELECTION | ~2 V fwd; series RES-D2 | In black Drive 2 front | red | TO_BE_SELECTED | REFERENCE | — | — | |
| LED-PWR | White mini LED | Powered-state indicator | 1 | TO_BE_SELECTED | PENDING_COMPONENT_SELECTION | White LED; series RES-PWR | Behind recessed POWER insert | white | TO_BE_SELECTED | REFERENCE | — | — | Not a switch |
| RES-D1 | Series resistor Drive1 LED | Current limit | 1 | Value TBD after LED+rail | — | TBD | Inline | — | TO_BE_SELECTED | NONE | — | — | |
| RES-D2 | Series resistor Drive2 LED | Current limit | 1 | Value TBD | — | TBD | Inline | — | TO_BE_SELECTED | NONE | — | — | |
| RES-PWR | Series resistor POWER LED | Current limit | 1 | Value TBD | — | TBD | Inline | — | TO_BE_SELECTED | NONE | — | — | |
| FAST-01 | M2 screws (board) | Retain Waveshare board | 4 | M2 length TBD after stackup | Match board holes (OFFICIAL M2 pattern) | — | Upper enclosure bosses | — | TO_BE_SELECTED | CRITICAL | — | — | Alignment via guides, not hole play |
| FAST-02 | Bottom-plate screws | Secure gray plate | TBD | TO_BE_SELECTED | TBD | — | Into beige upper | — | TO_BE_SELECTED | REFERENCE | — | — | |
| FAST-03 | Heat-set inserts (optional) | Threaded bosses | TBD | TO_BE_SELECTED | TBD | — | Printed bosses | — | TO_BE_SELECTED | REFERENCE | — | — | |
| PRT-BEIGE | Beige upper print | Shell | 1 | Filament TBD | CAD TBD | — | — | beige | TO_BE_SELECTED | CRITICAL | — | — | Single-color shell preferred |
| PRT-GRAY | Gray bottom plate | Service cover | 1 | Filament TBD | CAD TBD | — | Screws | gray | TO_BE_SELECTED | CRITICAL | — | — | |
| PRT-BROWN | Brown keyboard | One-piece keys | 1 | Filament TBD | CAD TBD | — | From below | brown | TO_BE_SELECTED | CRITICAL | — | — | |
| PRT-BLK-D1 | Black Drive 1 front | Drive face insert | 1 | Filament TBD | CAD TBD | — | Locate in beige recess | black | TO_BE_SELECTED | REFERENCE | — | — | |
| PRT-BLK-D2 | Black Drive 2 front | Drive face insert | 1 | Filament TBD | CAD TBD | — | Locate in beige recess | black | TO_BE_SELECTED | REFERENCE | — | — | |
| PRT-WHT-LEG | White key legends | Printable legend geometry | 1 set | 0.2 mm nozzle strategy | CAD TBD | — | Part of / bonded to keys TBD | white | TO_BE_SELECTED | REFERENCE | — | — | Not textures |
| PRT-WHT-BADGE | White badge carriers | Logo carriers | TBD | Placeholder geometry | CAD TBD | — | Shallow recesses | white | TO_BE_SELECTED | REFERENCE | — | — | No copyrighted artwork in repo CAD |
| PRT-PWR | POWER indicator insert | Recessed legend | 1 | White/translucent | CAD TBD | Illuminated by LED-PWR | From above | translucent | TO_BE_SELECTED | REFERENCE | — | — | |

### Audio rule (architecture)

**Never** connect headphones directly to an ESP32 GPIO.  
TRRS and headphone drive require an explicit, safe audio path (amplifier / codec / BLE audio) — assignment remains `UNASSIGNED` until architecture is finalized (`WIRING.md`).

---

## PADDLE — COMMON

| ID | Component | Function | Qty | Status | CAD dependency | Notes |
| --- | --- | --- | ---: | --- | --- | --- |
| PAD-SHELL | Upper + lower shell (1:1) | Exterior | 1 set / paddle | TO_BE_SELECTED (filament) | CRITICAL (later) | Shared by wired/wireless |
| PAD-KNOB | Rotary knob | Position control | 1 | TO_BE_SELECTED | CRITICAL | Physical validation vs author paddles |
| PAD-BTN-ORIG-REF | Original-style button | FIRE | 1 | TO_BE_SELECTED | CRITICAL | Geometry from photos/docs — **not** author-modified buttons |
| PAD-FAST | Shell screws | Assembly | TBD | TO_BE_SELECTED | REFERENCE | Match physical reference |
| PAD-CARRIER | Internal carrier interface | Holds electronics | 1 | TO_BE_SELECTED | CRITICAL | Swap wired vs wireless carriers |

## PADDLE — WIRED

| ID | Component | Function | Qty | Status | CAD dependency | Notes |
| --- | --- | --- | ---: | --- | --- | --- |
| PAD-W-POT | Position pot or encoder | Angle | 1 | PENDING_COMPONENT_SELECTION | CRITICAL | Historical ~150 kΩ; modern choice TBD |
| PAD-W-TRRS | 2.5 mm TRRS plug/cable | Host link | 1 | PENDING_COMPONENT_SELECTION | CRITICAL | Pinout not frozen |
| PAD-W-CARRIER | `wired_internal_carrier` | PCB/pot mount | 1 | TO_BE_SELECTED | CRITICAL | No battery |

## PADDLE — WIRELESS

| ID | Component | Function | Qty | Status | CAD dependency | Notes |
| --- | --- | --- | ---: | --- | --- | --- |
| PAD-WL-MCU | Mini ESP-family BLE module | Radio + logic | 1 | TO_BE_SELECTED | CRITICAL | Antenna keep-out TBD |
| PAD-WL-BAT | Small LiPo | Power | 1 | TO_BE_SELECTED | CRITICAL | Charge path TBD |
| PAD-WL-CHG | Charge / power | USB or inductive TBD | 1 | TO_BE_SELECTED | CRITICAL | |
| PAD-WL-SENS | Position sensor | Angle | 1 | TO_BE_SELECTED | CRITICAL | Pot or contactless |
| PAD-WL-CARRIER | `wireless_internal_carrier` | Mount | 1 | TO_BE_SELECTED | CRITICAL | |

---

## JOYSTICK — COMMON

| ID | Component | Function | Qty | Status | CAD dependency | Notes |
| --- | --- | --- | ---: | --- | --- | --- |
| JOY-SHELL | Base / cover (1:1) | Exterior | 1 | TO_BE_SELECTED | CRITICAL | Validate vs author joystick |
| JOY-STICK | Stick / knob | User grip | 1 | TO_BE_SELECTED | CRITICAL | |
| JOY-BTN | Fire buttons | Digital | 2 | TO_BE_SELECTED | CRITICAL | |
| JOY-SPRING-X | Centering spring X | Force only | 1 | TO_BE_SELECTED | CRITICAL | Not the position sensor |
| JOY-SPRING-Y | Centering spring Y | Force only | 1 | TO_BE_SELECTED | CRITICAL | |
| JOY-ACT-X | Spring engage actuator X | Switchable centering | 1 | CANDIDATE class | CRITICAL | Mechanism NOT_SELECTED |
| JOY-ACT-Y | Spring engage actuator Y | Switchable centering | 1 | CANDIDATE class | CRITICAL | Prefer low idle energy |
| JOY-SENS-XY | Contactless X/Y sensor | Absolute position | 1 set | TO_BE_SELECTED | CRITICAL | Hall / magnetic preferred |

## JOYSTICK — WIRED

| ID | Component | Function | Qty | Status | CAD dependency | Notes |
| --- | --- | --- | ---: | --- | --- | --- |
| JOY-W-MCU | Intelligent controller (preferred concept) | Sense + centering + host link | 1 | TO_BE_SELECTED | CRITICAL | Passive analog TRRS likely insufficient |
| JOY-W-LINK | Host connector / cable | Power + data | 1 | PENDING_COMPONENT_SELECTION | CRITICAL | TRRS digital concept unfrozen |
| JOY-W-CARRIER | Wired internal carrier | Mount | 1 | TO_BE_SELECTED | CRITICAL | |

## JOYSTICK — WIRELESS

| ID | Component | Function | Qty | Status | CAD dependency | Notes |
| --- | --- | --- | ---: | --- | --- | --- |
| JOY-WL-MCU | ESP-family BLE | Radio + logic | 1 | TO_BE_SELECTED | CRITICAL | |
| JOY-WL-BAT | LiPo | Power | 1 | TO_BE_SELECTED | CRITICAL | |
| JOY-WL-CHG | Charge | Power | 1 | TO_BE_SELECTED | CRITICAL | |
| JOY-WL-CARRIER | Wireless internal carrier | Mount | 1 | TO_BE_SELECTED | CRITICAL | Antenna keep-out |
