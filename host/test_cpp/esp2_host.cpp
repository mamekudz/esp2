/**
 * ESP][ host runner — console development front-end.
 *
 * Usage:
 *   esp2_host [--rom path] [--machine AppleII|AppleIIPlus]
 *             [--slot6 none|synthetic|cleanroom|path] [--slot6-rom path]
 *             [--disk1 id] [--cycles N] [--trace] [--text] [--diagnostics]
 *             [--script path] [--video-dump path.ppm]
 *
 * Without --rom: loads project synthetic ROM (always legal).
 * --slot6 <path> or --slot6-rom <path>: user-supplied 256-byte Disk II PROM.
 * Emulator commands (not Apple keys): quit, reset, text, diag, pause
 */
#include "esp_bracket/apple2_machine_host.hpp"
#include "esp_bracket/input_script.hpp"
#include "esp_bracket/key_map.hpp"
#include "esp_bracket/ppm.hpp"
#include "esp_bracket/text_decoder.hpp"
#include "esp_bracket/text_screen.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace esp_bracket;

static void printStatus(const HostAppleIIMachine &m) {
    const HostMachineSnapshot s = m.snapshot();
    std::printf("ESP][ HOST\n");
    std::printf("  ROM      %s (%s)\n", m.romIdentity().name,
                m.romIdentity().synthetic ? "synthetic" : romIdStatusName(m.romIdentity().status));
    std::printf("  PROFILE  %s\n", machineProfileName(m.machineProfile()));
    std::printf("  PC       %04X  cycles=%llu\n", s.pc, static_cast<unsigned long long>(s.cycles));
    std::printf("  VIDEO    %s%s page%d\n", s.text ? "TEXT" : (s.hires ? "HGR" : "LORES"),
                s.mixed ? "+MIXED" : "", s.page2 ? 2 : 1);
    std::printf("  SLOT6    %s\n",
                m.slot6RomMode() == Slot6RomMode::None
                    ? "none"
                    : (m.slot6RomMode() == Slot6RomMode::Synthetic
                           ? "synthetic"
                           : (m.slot6RomMode() == Slot6RomMode::CleanRoom ? "cleanroom" : "user")));
    std::printf("  DRIVE1   %s\n", m.drive(DriveId::Drive1).state().inserted
                                       ? m.drive(DriveId::Drive1).state().imageId
                                       : "(empty)");
}

static void printText(const HostAppleIIMachine &m) {
    TextScreen s = TextScreen::fromBus(m.bus(), m.flashPhase());
    char lines[24 * 41];
    s.toLines(lines, sizeof(lines));
    std::printf("---- TEXT ----\n%s\n--------------\n", lines);
}

static bool loadFile(const char *path, std::vector<uint8_t> *out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    f.seekg(0, std::ios::end);
    const auto n = f.tellg();
    if (n <= 0) {
        return false;
    }
    f.seekg(0, std::ios::beg);
    out->resize(static_cast<size_t>(n));
    f.read(reinterpret_cast<char *>(out->data()), n);
    return static_cast<bool>(f) || f.eof();
}

static void dumpVideoPpm(HostAppleIIMachine &m, const char *path) {
    TextScreen s = TextScreen::fromBus(m.bus(), m.flashPhase());
    uint8_t chars[40 * 24];
    for (int r = 0; r < 24; ++r) {
        for (int c = 0; c < 40; ++c) {
            chars[r * 40 + c] = static_cast<uint8_t>(s.cells[r][c].ch | 0x80);
        }
    }
    std::vector<uint8_t> rgb(TextDecoder::kRgbW * TextDecoder::kRgbH * 3);
    TextDecoder::renderRgb888(chars, rgb.data(), rgb.size());
    Ppm::writeP6(path, TextDecoder::kRgbW, TextDecoder::kRgbH, rgb.data(), rgb.size());
    std::printf("wrote %s\n", path);
}

int main(int argc, char **argv) {
    const char *romPath = nullptr;
    const char *machineArg = nullptr;
    const char *slot6Arg = "none";
    const char *slot6RomPath = nullptr;
    const char *disk1 = nullptr;
    const char *videoDump = nullptr;
    uint32_t bootCycles = 200000;
    bool trace = false;
    bool showText = false;
    bool diagnostics = false;
    bool interactive = true;

    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
            romPath = argv[++i];
        } else if (std::strcmp(argv[i], "--machine") == 0 && i + 1 < argc) {
            machineArg = argv[++i];
        } else if (std::strcmp(argv[i], "--slot6") == 0 && i + 1 < argc) {
            slot6Arg = argv[++i];
        } else if (std::strcmp(argv[i], "--slot6-rom") == 0 && i + 1 < argc) {
            slot6RomPath = argv[++i];
        } else if (std::strcmp(argv[i], "--disk1") == 0 && i + 1 < argc) {
            disk1 = argv[++i];
        } else if (std::strcmp(argv[i], "--cycles") == 0 && i + 1 < argc) {
            bootCycles = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (std::strcmp(argv[i], "--trace") == 0) {
            trace = true;
        } else if (std::strcmp(argv[i], "--text") == 0) {
            showText = true;
        } else if (std::strcmp(argv[i], "--diagnostics") == 0) {
            diagnostics = true;
        } else if (std::strcmp(argv[i], "--video-dump") == 0 && i + 1 < argc) {
            videoDump = argv[++i];
        } else if (std::strcmp(argv[i], "--batch") == 0) {
            interactive = false;
        } else if (std::strcmp(argv[i], "--help") == 0) {
            std::printf("esp2_host [--rom path] [--machine AppleII|AppleIIPlus] "
                        "[--slot6 none|synthetic|cleanroom|<path>] [--slot6-rom path] "
                        "[--disk1 Esp2BootTest|Esp2DiskTest] "
                        "[--cycles N] [--text] [--diagnostics] [--batch]\n");
            return 0;
        }
    }
    (void)trace; // reserved: per-instruction trace off by default

    HostAppleIIMachine m;
    if (machineArg) {
        if (std::strcmp(machineArg, "AppleII") == 0) {
            m.setMachineProfile(MachineProfile::AppleII);
        } else if (std::strcmp(machineArg, "AppleIIPlus") == 0) {
            m.setMachineProfile(MachineProfile::AppleIIPlus);
        }
    }

    if (romPath) {
        std::vector<uint8_t> bytes;
        if (!loadFile(romPath, &bytes)) {
            std::fprintf(stderr, "FAIL rom_open path=%s\n", romPath);
            return 2;
        }
        RomIdentity id{};
        const RomError err = m.loadRomIdentified(bytes.data(), bytes.size(), &id);
        std::printf("ROM status=%s sha256=%s name=%s profile=%s\n", romIdStatusName(id.status),
                    id.sha256Hex, id.name, machineProfileName(id.profile));
        if (err != RomError::Ok) {
            std::fprintf(stderr, "FAIL rom_load\n");
            return 3;
        }
    } else {
        if (m.loadSyntheticRom() != RomError::Ok) {
            std::fprintf(stderr, "FAIL synthetic_rom\n");
            return 1;
        }
        std::printf("ROM synthetic sha256=%s\n", m.romIdentity().sha256Hex);
    }

    if (slot6RomPath) {
        slot6Arg = slot6RomPath;
    }

    if (std::strcmp(slot6Arg, "synthetic") == 0) {
        m.setSlot6RomMode(Slot6RomMode::Synthetic);
    } else if (std::strcmp(slot6Arg, "cleanroom") == 0) {
        m.setSlot6RomMode(Slot6RomMode::CleanRoom);
    } else if (std::strcmp(slot6Arg, "none") == 0) {
        m.setSlot6RomMode(Slot6RomMode::None);
    } else {
        // Treat as filesystem path to a 256-byte Slot-6 PROM.
        std::vector<uint8_t> bytes;
        if (!loadFile(slot6Arg, &bytes)) {
            std::fprintf(stderr, "FAIL slot6_rom_open path=%s status=SKIPPED_NO_SLOT6_ROM\n",
                         slot6Arg);
            return 4;
        }
        Slot6RomIdentity sid{};
        const RomError serr = m.loadSlot6UserRom(bytes.data(), bytes.size(), &sid);
        std::printf("SLOT6 status=%s sha256=%s name=%s size=%zu\n", romIdStatusName(sid.status),
                    sid.sha256Hex, sid.name, sid.sizeBytes);
        if (serr != RomError::Ok) {
            std::fprintf(stderr, "FAIL slot6_rom_load\n");
            return 5;
        }
    }

    if (disk1) {
        if (m.mountDisk(DriveId::Drive1, disk1) != MediaResult::Ok) {
            std::fprintf(stderr, "WARN disk1 mount failed\n");
        }
    }

    m.powerOn(RamInitMode::Zero);
    m.runCycles(bootCycles);

    if (diagnostics) {
        printStatus(m);
    }
    if (showText) {
        printText(m);
    }
    if (videoDump) {
        dumpVideoPpm(m, videoDump);
    }

    if (!interactive) {
        return 0;
    }

    printStatus(m);
    std::printf("Commands: quit | reset | text | diag | run N | type TEXT\n");
    std::printf("Other lines: first char injected as Apple II key.\n");

    char line[512];
    while (std::fgets(line, sizeof(line), stdin)) {
        if (std::strncmp(line, "quit", 4) == 0) {
            break;
        }
        if (std::strncmp(line, "reset", 5) == 0) {
            m.reset();
            continue;
        }
        if (std::strncmp(line, "text", 4) == 0) {
            printText(m);
            continue;
        }
        if (std::strncmp(line, "diag", 4) == 0) {
            printStatus(m);
            continue;
        }
        if (std::strncmp(line, "run ", 4) == 0) {
            m.runCycles(static_cast<uint32_t>(std::strtoul(line + 4, nullptr, 10)));
            continue;
        }
        if (std::strncmp(line, "type ", 5) == 0) {
            ScriptStep st{ScriptOp::Type, line + 5, 0};
            // strip newline
            char *nl = std::strchr(line + 5, '\n');
            if (nl) {
                *nl = '\0';
            }
            InputScript::run(m, &st, 1);
            continue;
        }
        if (line[0] && line[0] != '\n') {
            uint8_t apple = 0;
            if (AppleIIKeyMap::mapHostKey(static_cast<unsigned char>(line[0]), false, false,
                                          &apple)) {
                m.keyDown(apple);
                m.runCycles(1000);
            }
        }
    }
    return 0;
}
