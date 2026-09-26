# ESP][ physical wiring plan

Authoritative human-readable connection plan. Machine-readable companion:
`dimensions/wiring.json`.

Statuses: `VERIFIED` `DOCUMENTED` `PROPOSED` `UNASSIGNED` `UNKNOWN`

**Do not invent GPIO assignments.** Waveshare board wiring + working ESP][
firmware are authoritative — not a generic ESP32-S3 pinout alone.

## 1. Authority sources

| Source | Role | Status |
| --- | --- | --- |
| `include/board_pins.h` | Pins used by ESP][ bring-up / apple2_text | VERIFIED |
| Arduino-ESP32 `waveshare_esp32_s3_touch_amoled_164/pins_arduino.h` | Vendor variant map | DOCUMENTED |
| Waveshare wiki + `ESP32-S3-Touch-AMOLED-1.64-schematic.pdf` / Rev1.1 | Onboard nets | DOCUMENTED |
| Physical board on desk | Runtime confirmation | PHYSICALLY_VERIFIED (subsystems) |

## 2. Occupied onboard functions

| Subsystem | Interface | GPIOs / notes | Status |
| --- | --- | --- | --- |
| CO5300 AMOLED | QSPI | CS=9 (V1) / 46 (V2), SCK=10, D0–D3=11–14, RST=21 | VERIFIED |
| FT3168 touch | I2C 0x38 | SDA=47, SCL=48, INT=18; RST shared with LCD_RST via R35 | VERIFIED |
| QMI8658 IMU | I2C 0x6A/0x6B | Shares 47/48; INT1=46 (V1) / 9 (V2) | VERIFIED (probe) |
| microSD | SPI | CS=38, MOSI=39, MISO=40, SCLK=41 | VERIFIED |
| USB | Native USB-C | HW CDC default; TinyUSB OTG if `ARDUINO_USB_MODE=0` | VERIFIED |
| Battery sense | ADC | GPIO4 (`BAT_ADC`) per variant | DOCUMENTED |
| BOOT button | Strap | GPIO0 | DOCUMENTED |
| PSRAM / Flash | Internal SPI | Occupied by chip; not for I/O | DOCUMENTED |
| Charging | Onboard charger → MX1.25 | Passive | DOCUMENTED |

PCB V1 vs V2: **LCD_CS ↔ IMU_INT1** swap (GPIO9 ↔ GPIO46). ESP][ defaults **V1**.

## 3. External functions (new enclosure)

| Function | Intent | GPIO | Driver / notes | Status |
| --- | --- | --- | --- | --- |
| DRIVE1_LED | Red activity LED | — | Series RES-D1 | UNASSIGNED |
| DRIVE2_LED | Red activity LED | — | Series RES-D2 | UNASSIGNED |
| POWER_LED | White powered-state LED | — | May be rail-tied or GPIO; RES-PWR | UNASSIGNED |
| AUDIO_INTERNAL | Piezo / small speaker | — | Safe drive path TBD; use vent slots as acoustic outlet | UNASSIGNED |
| AUDIO_TRRS | 2.5 mm 4-pole panel | — | Pinout undecided; **no headphones on bare GPIO** | UNASSIGNED |
| POWER_SWITCH | Rear mechanical ON/OFF | — | Likely series power path, not a GPIO | UNASSIGNED |
| BATTERY | LiPo pack | GPIO4 sense | Charge path onboard Waveshare | PROPOSED |
| USB panel | Chassis USB-C | N/A (passthrough) | Adapter + extension from module USB-C | PROPOSED |

## 4. Audio architecture constraints

1. Apple II speaker remains **cycle-accurate 1-bit edges** in software until PCM / BlueShift (CLAUDE.md §17).
2. Local transducer = simple piezo/speaker for built-in / fallback / debug.
3. Bluetooth headphones are a separate V1 software goal — not assumed available as Classic A2DP on ESP32-S3 without experimental proof.
4. **Never** wire TRS/TRRS headphones directly to an ESP32 GPIO.
5. Any headphone/line output needs an explicit amplifier / codec / BLE audio path before GPIO assignment.

## 5. Header pin map

Module exposes **2×12** 2.54 mm headers (OFFICIAL mechanical dims in `board.json`).

Exact **GPIO ↔ header pin** table: **UNKNOWN** until extracted from Waveshare schematic / JW soldering guide and cross-checked on hardware. Placeholder: do not assign enclosure LEDs/audio until that map is `DOCUMENTED` or `VERIFIED`.

## 6. Open work

- [ ] Extract header pinout table from Rev1.1 schematic into `wiring.json`
- [ ] Select free GPIOs for DRIVE1/2 LEDs (avoid strapping pins)
- [ ] Decide POWER_LED = GPIO vs always-on with switch
- [ ] Select piezo driver topology
- [ ] Finalize TRRS pinout with paddle/joystick/audio architecture
- [ ] Document USB-C extension integrity (D+/D− length, shielding)

## 7. Accessory interfaces (conceptual — not frozen)

Accessories are **1:1 original size** and connect as external devices — see
`paddles/` and `joystick/`. Do **not** invent TRRS pinouts or GPIOs here.

### Paddle — wired (concept)

| Signal class | Notes | Status |
| --- | --- | --- |
| Position | Analog or digitized at paddle | UNASSIGNED |
| Button | Digital | UNASSIGNED |
| Power / GND | From host | PROPOSED |
| Connector | 2.5 mm TRRS candidate vs dedicated plug | PENDING_COMPONENT_SELECTION |

### Paddle — wireless (concept)

BLE HID / custom report; LiPo + charge local to paddle. No TRRS required.

### Joystick — wired (concept)

Passive analog multi-signal TRRS is **probably insufficient** if centering
control + power + X/Y + buttons are required. Preferred unfrozen concept:

| Option | Description | Status |
| --- | --- | --- |
| A | Legacy-style multi-pole analog | RESEARCH |
| B | Passive analog TRRS only | PROBABLY_INSUFFICIENT |
| C | Intelligent in-stick MCU; **power + bidirectional digital** over slim connector (TRRS candidate) | PREFERRED_CONCEPT (unfrozen) |

### Joystick — wireless (concept)

BLE + local LiPo; centering actuators prefer pulse/bistable energy (not continuous hold).

### Centering control (joystick)

Electronically switchable X/Y spring engagement is a **device-local** function.
Host protocol may later expose mode commands — assignment `UNASSIGNED`.
