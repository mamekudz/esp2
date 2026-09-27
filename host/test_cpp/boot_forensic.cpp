/**
 * HOST-ONLY Apple II boot forensic tracer (generic).
 *
 * Usage:
 *   boot_forensic --rom PATH --disk PATH [--boot-disk] [--post-handoff N]
 *                 [--ram-init zero|ones|random] [--out-dir local/apple2/forensics]
 *
 * Captures a bounded ring trace from natural Slot-6 boot handoff into loaded
 * RAM. Does not modify emulator semantics. Local dumps stay under gitignored
 * local/apple2/.
 */
#include "esp_bracket/apple2_machine_host.hpp"
#include "esp_bracket/disk_ii_cleanroom.hpp"
#include "esp_bracket/sha256.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace esp_bracket;

namespace {

constexpr size_t kRing = 4096;
constexpr int kShadowDepth = 64;

struct TraceRec {
    uint64_t cycle = 0;
    uint16_t pc = 0;
    uint8_t op = 0;
    uint8_t b1 = 0;
    uint8_t b2 = 0;
    uint8_t a = 0, x = 0, y = 0, sp = 0, p = 0;
    uint8_t kind = 0; // 0=other 1=JSR 2=RTS 3=JMP 4=BRK 5=branch 6=RTI
    uint16_t target = 0;
    uint16_t shadowRet = 0;
    int shadowDepth = 0;
    uint8_t spBefore = 0;
    uint16_t memAddr = 0;
    uint8_t memVal = 0;
    uint8_t memOp = 0; // 0=none 1=read-ish 2=write observed via RAM delta scan skip
};

struct ShadowFrame {
    uint16_t ret = 0;
    uint16_t jsrPc = 0;
    uint8_t spAtJsr = 0;
};

struct OpcodeStat {
    uint32_t count = 0;
    uint16_t firstPc = 0;
    bool seen = false;
};

bool loadFile(const char *path, std::vector<uint8_t> *out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    f.seekg(0, std::ios::end);
    const auto n = f.tellg();
    f.seekg(0, std::ios::beg);
    out->resize(static_cast<size_t>(n));
    f.read(reinterpret_cast<char *>(out->data()), n);
    return true;
}

const char *opcClass(uint8_t op) {
    // Documented NMOS vs common undocumented vs KIL
    static const char *kKil = "KIL";
    static const char *kUnd = "UND";
    static const char *kDoc = "DOC";
    // KIL/JAM: 02 12 22 32 42 52 62 72 92 B2 D2 F2
    if (op == 0x02 || op == 0x12 || op == 0x22 || op == 0x32 || op == 0x42 || op == 0x52 ||
        op == 0x62 || op == 0x72 || op == 0x92 || op == 0xB2 || op == 0xD2 || op == 0xF2) {
        return kKil;
    }
    // Undocumented (non-NOP aliases that do real work / unstable)
    // Includes SBX CB, SHY 9C, SHX 9E, SAX, LAX, DCP, ISB, SLO, RLA, SRE, RRA, ANC, ALR, ARR, XAA,
    // TAS, LAS, AHX
    const uint8_t und[] = {0x03, 0x07, 0x0B, 0x0F, 0x13, 0x17, 0x1B, 0x1F, 0x23, 0x27, 0x2B,
                           0x2F, 0x33, 0x37, 0x3B, 0x3F, 0x43, 0x47, 0x4B, 0x4F, 0x53, 0x57,
                           0x5B, 0x5F, 0x63, 0x67, 0x6B, 0x6F, 0x73, 0x77, 0x7B, 0x7F, 0x83,
                           0x87, 0x8B, 0x8F, 0x93, 0x97, 0x9B, 0x9C, 0x9E, 0x9F, 0xA3, 0xA7,
                           0xAB, 0xAF, 0xB3, 0xB7, 0xBB, 0xBF, 0xC3, 0xC7, 0xCB, 0xCF, 0xD3,
                           0xD7, 0xDB, 0xDF, 0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF};
    for (uint8_t u : und) {
        if (u == op) {
            return kUnd;
        }
    }
    // NOP undocumented immediates/abs still "und" lightly — treat $80,$82,$C2,$E2,$89 as UND-NOP
    if (op == 0x80 || op == 0x82 || op == 0xC2 || op == 0xE2 || op == 0x89 || op == 0x1A ||
        op == 0x3A || op == 0x5A || op == 0x7A || op == 0xDA || op == 0xFA || op == 0x04 ||
        op == 0x14 || op == 0x34 || op == 0x44 || op == 0x54 || op == 0x64 || op == 0x74 ||
        op == 0xD4 || op == 0xF4 || op == 0x0C || op == 0x1C || op == 0x3C || op == 0x5C ||
        op == 0x7C || op == 0xDC || op == 0xFC) {
        return kUnd;
    }
    return kDoc;
}

uint8_t flowKind(uint8_t op) {
    if (op == 0x20) {
        return 1; // JSR
    }
    if (op == 0x60) {
        return 2; // RTS
    }
    if (op == 0x4C || op == 0x6C) {
        return 3; // JMP
    }
    if (op == 0x00) {
        return 4; // BRK
    }
    if (op == 0x40) {
        return 6; // RTI
    }
    if (op == 0x10 || op == 0x30 || op == 0x50 || op == 0x70 || op == 0x90 || op == 0xB0 ||
        op == 0xD0 || op == 0xF0) {
        return 5;
    }
    return 0;
}

} // namespace

int main(int argc, char **argv) {
    const char *romPath = "local/apple2/roms/appleiigo.rom";
    const char *diskPath = "local/apple2/disks/Galaxian.dsk";
    const char *outDir = "local/apple2/forensics";
    bool bootDisk = true;
    int postHandoff = 512;
    uint32_t maxBootCycles = 500000;
    RamInitMode ramInit = RamInitMode::Zero;

    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--rom") && i + 1 < argc) {
            romPath = argv[++i];
        } else if (!std::strcmp(argv[i], "--disk") && i + 1 < argc) {
            diskPath = argv[++i];
        } else if (!std::strcmp(argv[i], "--out-dir") && i + 1 < argc) {
            outDir = argv[++i];
        } else if (!std::strcmp(argv[i], "--boot-disk")) {
            bootDisk = true;
        } else if (!std::strcmp(argv[i], "--no-boot-disk")) {
            bootDisk = false;
        } else if (!std::strcmp(argv[i], "--post-handoff") && i + 1 < argc) {
            postHandoff = static_cast<int>(std::strtol(argv[++i], nullptr, 10));
        } else if (!std::strcmp(argv[i], "--max-boot-cycles") && i + 1 < argc) {
            maxBootCycles = static_cast<uint32_t>(std::strtoul(argv[++i], nullptr, 10));
        } else if (!std::strcmp(argv[i], "--ram-init") && i + 1 < argc) {
            ++i;
            if (!std::strcmp(argv[i], "zero")) {
                ramInit = RamInitMode::Zero;
            } else if (!std::strcmp(argv[i], "ones")) {
                ramInit = RamInitMode::Ones;
            } else if (!std::strcmp(argv[i], "random")) {
                ramInit = RamInitMode::Random;
            } else {
                std::fprintf(stderr, "FAIL --ram-init zero|ones|random\n");
                return 1;
            }
        }
    }

#ifdef _WIN32
    std::string mk = std::string("mkdir \"") + outDir + "\" 2>nul";
    std::system(mk.c_str());
#else
    std::string mk = std::string("mkdir -p \"") + outDir + "\"";
    std::system(mk.c_str());
#endif

    std::vector<uint8_t> rom;
    std::vector<uint8_t> disk;
    if (!loadFile(romPath, &rom) || !loadFile(diskPath, &disk)) {
        std::fprintf(stderr, "FAIL load rom/disk\n");
        return 2;
    }
    char romHex[65], diskHex[65];
    Sha256::hashHex(rom.data(), rom.size(), romHex);
    Sha256::hashHex(disk.data(), disk.size(), diskHex);
    std::printf("ROM size=%zu sha256=%s\n", rom.size(), romHex);
    std::printf("DISK size=%zu sha256=%s\n", disk.size(), diskHex);

    HostAppleIIMachine m;
    RomIdentity id{};
    if (m.loadRomIdentified(rom.data(), rom.size(), &id) != RomError::Ok) {
        std::fprintf(stderr, "FAIL rom\n");
        return 3;
    }
    m.setSlot6RomMode(Slot6RomMode::CleanRoom);
    if (m.mountDisk(DriveId::Drive1, diskPath) != MediaResult::Ok) {
        std::fprintf(stderr, "FAIL mount\n");
        return 4;
    }
    m.powerOn(ramInit);
    std::printf("RAM_INIT mode=%u (0=Zero 1=Ones 2=Random)\n", static_cast<unsigned>(ramInit));
    if (bootDisk) {
        CpuRegisters r = m.cpu().registers();
        std::printf("RESET pc=%04X\n", r.pc);
        r.pc = 0xC600;
        m.cpu().setRegisters(r);
        std::printf("ENTRY pc=C600 boot-disk\n");
    }

    TraceRec ring[kRing];
    size_t ringPos = 0, ringCount = 0;
    ShadowFrame shadow[kShadowDepth];
    int shadowTop = 0;
    OpcodeStat opc[256]{};
    uint32_t romCalls = 0;
    uint16_t firstRomCall = 0, firstRomCaller = 0;
    uint32_t romDataOps = 0;
    uint16_t firstRomDataPc = 0, firstRomDataEa = 0;
    uint32_t smcWrites = 0;
    uint16_t firstSmcAddr = 0;
    bool handoffSeen = false;
    TraceRec handoff{};
    int postLeft = -1;
    uint8_t prevBpage[0x1000];
    std::memset(prevBpage, 0, sizeof(prevBpage));
    bool prevBpageInit = false;
    uint32_t stackAnomaly = 0;
    uint16_t firstStackAnomalyPc = 0;
    uint32_t unmatchedRts = 0;
    uint16_t firstUnmatchedRts = 0;

    auto pushRing = [&](const TraceRec &tr) {
        ring[ringPos] = tr;
        ringPos = (ringPos + 1) % kRing;
        if (ringCount < kRing) {
            ++ringCount;
        }
    };

    // Run until handoff (JMP $B100 at $0842 or first PC in $B100-$B1FF after sectors),
    // then single-step postHandoff instructions.
    for (uint32_t i = 0; i < maxBootCycles; ++i) {
        const bool fine =
            handoffSeen || (m.cpu().registers().pc >= 0x0800 && m.cpu().registers().pc < 0x0900);
        if (!handoffSeen) {
            m.runCycles(fine ? 1 : 32);
        }

        CpuRegisters r = m.cpu().registers();
        const uint8_t *ram = m.bus().ram();

        // Detect handoff: boot0 JMP $B100 instruction at $0842
        if (!handoffSeen && r.pc == 0x0842 && ram[0x0842] == 0x4C && ram[0x0843] == 0x00 &&
            ram[0x0844] == 0xB1) {
            handoffSeen = true;
            handoff.cycle = m.cpu().cycles();
            handoff.pc = r.pc;
            handoff.a = r.a;
            handoff.x = r.x;
            handoff.y = r.y;
            handoff.sp = r.sp;
            handoff.p = r.status;
            handoff.op = ram[r.pc];
            handoff.b1 = ram[r.pc + 1];
            handoff.b2 = ram[r.pc + 2];
            std::printf("\n=== HANDOFF $0842 JMP $B100 ===\n");
            std::printf("cycle=%llu A=%02X X=%02X Y=%02X SP=%02X P=%02X\n",
                        (unsigned long long)handoff.cycle, handoff.a, handoff.x, handoff.y,
                        handoff.sp, handoff.p);
            std::printf("patches B10B=%02X B10C=%02X B8CB=%02X\n", ram[0xB10B], ram[0xB10C],
                        ram[0xB8CB]);
            std::printf("ZP 26=%02X 27=%02X 2B=%02X 3D=%02X 41=%02X\n", ram[0x26], ram[0x27],
                        ram[0x2B], ram[0x3D], ram[0x41]);
            // Metadata-only fixture (no copyrighted code bytes)
            char path[512];
            std::snprintf(path, sizeof(path), "%s/handoff_meta.txt", outDir);
            if (FILE *hf = std::fopen(path, "w")) {
                std::fprintf(hf, "pc=%04X\nA=%02X\nX=%02X\nY=%02X\nSP=%02X\nP=%02X\ncycle=%llu\n",
                             handoff.pc, handoff.a, handoff.x, handoff.y, handoff.sp, handoff.p,
                             (unsigned long long)handoff.cycle);
                std::fprintf(hf, "B10B=%02X\nB10C=%02X\nB8CB=%02X\n", ram[0xB10B], ram[0xB10C],
                             ram[0xB8CB]);
                std::fprintf(hf, "zp26=%02X\nzp27=%02X\nzp2B=%02X\nzp3D=%02X\nzp41=%02X\n",
                             ram[0x26], ram[0x27], ram[0x2B], ram[0x3D], ram[0x41]);
                std::fprintf(hf, "rom_sha256=%s\ndisk_sha256=%s\n", romHex, diskHex);
                std::fclose(hf);
            }
            // Local full B0-BF hash only (not committed)
            char regionHex[65];
            Sha256::hashHex(ram + 0xB000, 0x1000, regionHex);
            std::snprintf(path, sizeof(path), "%s/ram_b000_bfff.sha256", outDir);
            if (FILE *hf = std::fopen(path, "w")) {
                std::fprintf(hf, "%s\n", regionHex);
                std::fclose(hf);
            }
            std::memcpy(prevBpage, ram + 0xB000, 0x1000);
            prevBpageInit = true;
            postLeft = postHandoff;
            std::printf("tracing next %d instructions\n", postHandoff);
            continue;
        }

        if (!handoffSeen) {
            if (m.cpu().cycles() > maxBootCycles * 2ull) {
                break;
            }
            continue;
        }

        // Post-handoff: single-step via cpu.step + bus sync
        r = m.cpu().registers();
        TraceRec tr{};
        tr.cycle = m.cpu().cycles();
        tr.pc = r.pc;
        tr.op = ram[r.pc];
        tr.b1 = ram[(r.pc + 1) & 0xFFFF];
        tr.b2 = ram[(r.pc + 2) & 0xFFFF];
        tr.a = r.a;
        tr.x = r.x;
        tr.y = r.y;
        tr.sp = r.sp;
        tr.p = r.status;
        tr.spBefore = r.sp;
        tr.kind = flowKind(tr.op);
        tr.shadowDepth = shadowTop;

        if (!opc[tr.op].seen) {
            opc[tr.op].seen = true;
            opc[tr.op].firstPc = tr.pc;
        }
        ++opc[tr.op].count;

        if (tr.pc >= 0xD000) {
            if (romCalls == 0) {
                firstRomCall = tr.pc;
                // caller approx previous ring
            }
            ++romCalls;
        }

        // Absolute / abs,X / abs,Y operand in $D000+ (data fingerprint of motherboard ROM)
        if (tr.op == 0x0D || tr.op == 0x0E || tr.op == 0x0F || tr.op == 0x1D || tr.op == 0x1E ||
            tr.op == 0x1F || tr.op == 0x2D || tr.op == 0x2E || tr.op == 0x2F || tr.op == 0x3D ||
            tr.op == 0x3E || tr.op == 0x3F || tr.op == 0x4D || tr.op == 0x4E || tr.op == 0x4F ||
            tr.op == 0x5D || tr.op == 0x5E || tr.op == 0x5F || tr.op == 0x6D || tr.op == 0x6E ||
            tr.op == 0x6F || tr.op == 0x7D || tr.op == 0x7E || tr.op == 0x7F || tr.op == 0x8D ||
            tr.op == 0x8E || tr.op == 0x8F || tr.op == 0x9D || tr.op == 0x9E || tr.op == 0x9F ||
            tr.op == 0xAD || tr.op == 0xAE || tr.op == 0xAF || tr.op == 0xBD || tr.op == 0xBE ||
            tr.op == 0xBF || tr.op == 0xCD || tr.op == 0xCE || tr.op == 0xCF || tr.op == 0xDD ||
            tr.op == 0xDE || tr.op == 0xDF || tr.op == 0xED || tr.op == 0xEE || tr.op == 0xEF ||
            tr.op == 0xFD || tr.op == 0xFE || tr.op == 0xFF || tr.op == 0x19 || tr.op == 0x39 ||
            tr.op == 0x59 || tr.op == 0x79 || tr.op == 0x99 || tr.op == 0xB9 || tr.op == 0xD9 ||
            tr.op == 0xF9 || tr.op == 0x1B || tr.op == 0x3B || tr.op == 0x5B || tr.op == 0x7B ||
            tr.op == 0xDB || tr.op == 0xFB) {
            const uint16_t base =
                static_cast<uint16_t>(tr.b1 | (static_cast<uint16_t>(tr.b2) << 8));
            if (base >= 0xD000) {
                if (!firstRomDataPc) {
                    firstRomDataPc = tr.pc;
                    firstRomDataEa = base;
                }
                ++romDataOps;
            }
        }

        if (tr.kind == 1) { // JSR
            const uint16_t tgt = static_cast<uint16_t>(tr.b1 | (static_cast<uint16_t>(tr.b2) << 8));
            tr.target = tgt;
            // 6502 pushes pc+2 (address of last operand), RTS increments → pc+3
            if (shadowTop < kShadowDepth) {
                shadow[shadowTop].jsrPc = tr.pc;
                shadow[shadowTop].ret = static_cast<uint16_t>(tr.pc + 3);
                shadow[shadowTop].spAtJsr = r.sp;
                tr.shadowRet = shadow[shadowTop].ret;
                ++shadowTop;
            }
            if (tgt >= 0xD000 && romCalls == 0) {
                firstRomCall = tgt;
                firstRomCaller = tr.pc;
            }
        } else if (tr.kind == 2) { // RTS
            if (shadowTop > 0) {
                --shadowTop;
                tr.shadowRet = shadow[shadowTop].ret;
                tr.target = shadow[shadowTop].ret;
            } else {
                ++unmatchedRts;
                if (!firstUnmatchedRts) {
                    firstUnmatchedRts = tr.pc;
                }
            }
        } else if (tr.kind == 3) {
            if (tr.op == 0x4C) {
                tr.target = static_cast<uint16_t>(tr.b1 | (static_cast<uint16_t>(tr.b2) << 8));
            }
        }

        pushRing(tr);

        // Execute one instruction
        m.cpu().step();
        // Keep Disk II cycle sync
        // (service no longer needed post-boot0, but harmless)
        serviceCleanRoomCardRequests(m.bus().ram(), m.diskII());

        CpuRegisters r2 = m.cpu().registers();
        // Stack anomaly: JSR should decrease SP by 2
        if (tr.kind == 1) {
            const uint8_t expectSp = static_cast<uint8_t>(tr.spBefore - 2);
            if (r2.sp != expectSp) {
                if (!firstStackAnomalyPc) {
                    firstStackAnomalyPc = tr.pc;
                }
                ++stackAnomaly;
            }
            // Verify return address bytes on stack
            const uint8_t lo = ram[0x0100 + static_cast<uint8_t>(r2.sp + 1)];
            const uint8_t hi = ram[0x0100 + static_cast<uint8_t>(r2.sp + 2)];
            const uint16_t pushed = static_cast<uint16_t>(lo | (static_cast<uint16_t>(hi) << 8));
            const uint16_t expectPush = static_cast<uint16_t>(tr.pc + 2);
            if (pushed != expectPush) {
                if (!firstStackAnomalyPc) {
                    firstStackAnomalyPc = tr.pc;
                }
                ++stackAnomaly;
                std::printf("STACK_BAD jsr@%04X pushed=%04X expect=%04X\n", tr.pc, pushed,
                            expectPush);
            }
        }
        if (tr.kind == 2 && tr.shadowRet) {
            if (r2.pc != tr.shadowRet) {
                if (!firstStackAnomalyPc) {
                    firstStackAnomalyPc = tr.pc;
                }
                ++stackAnomaly;
                std::printf("RTS_BAD @%04X got=%04X shadow=%04X\n", tr.pc, r2.pc, tr.shadowRet);
            }
        }

        // SMC detection in $B000-$BFFF
        if (prevBpageInit) {
            for (int off = 0; off < 0x1000; ++off) {
                if (ram[0xB000 + off] != prevBpage[off]) {
                    if (!firstSmcAddr) {
                        firstSmcAddr = static_cast<uint16_t>(0xB000 + off);
                        std::printf("SMC first write addr=%04X old=%02X new=%02X after_pc=%04X "
                                    "from_instr=%04X\n",
                                    firstSmcAddr, prevBpage[off], ram[0xB000 + off], r2.pc, tr.pc);
                    }
                    ++smcWrites;
                }
            }
            std::memcpy(prevBpage, ram + 0xB000, 0x1000);
        }

        if (--postLeft <= 0) {
            break;
        }
    }

    if (!handoffSeen) {
        const auto s = m.snapshot();
        std::printf("FAIL no handoff pc=%04X cycles=%llu\n", s.pc, (unsigned long long)s.cycles);
        return 5;
    }

    // Print ring (oldest first)
    std::printf("\n=== TRACE (%zu records, last %d post-handoff) ===\n", ringCount, postHandoff);
    const size_t start = (ringCount < kRing) ? 0 : ringPos;
    char tpath[512];
    std::snprintf(tpath, sizeof(tpath), "%s/trace_ring.txt", outDir);
    FILE *tf = std::fopen(tpath, "w");
    for (size_t n = 0; n < ringCount; ++n) {
        const TraceRec &tr = ring[(start + n) % kRing];
        char line[256];
        std::snprintf(line, sizeof(line),
                      "%04zu cyc=%llu pc=%04X op=%02X %02X %02X A=%02X X=%02X Y=%02X SP=%02X "
                      "P=%02X k=%u tgt=%04X sh=%d %s\n",
                      n, (unsigned long long)tr.cycle, tr.pc, tr.op, tr.b1, tr.b2, tr.a, tr.x, tr.y,
                      tr.sp, tr.p, tr.kind, tr.target, tr.shadowDepth, opcClass(tr.op));
        if (n < 80 || tr.kind != 0) {
            std::fputs(line, stdout);
        }
        if (tf) {
            std::fputs(line, tf);
        }
    }
    if (tf) {
        std::fclose(tf);
    }

    // Opcode inventory
    std::printf("\n=== OPCODE INVENTORY (post-handoff window) ===\n");
    uint32_t docN = 0, undN = 0, kilN = 0, docTypes = 0, undTypes = 0;
    std::snprintf(tpath, sizeof(tpath), "%s/opcodes.txt", outDir);
    tf = std::fopen(tpath, "w");
    for (int op = 0; op < 256; ++op) {
        if (!opc[op].count) {
            continue;
        }
        const char *cls = opcClass(static_cast<uint8_t>(op));
        if (cls[0] == 'D') {
            docN += opc[op].count;
            ++docTypes;
        } else if (cls[0] == 'U') {
            undN += opc[op].count;
            ++undTypes;
            std::printf("UND %02X count=%u firstPC=%04X\n", op, opc[op].count, opc[op].firstPc);
        } else {
            kilN += opc[op].count;
            std::printf("KIL %02X count=%u firstPC=%04X\n", op, opc[op].count, opc[op].firstPc);
        }
        if (tf) {
            std::fprintf(tf, "%02X %s count=%u firstPC=%04X\n", op, cls, opc[op].count,
                         opc[op].firstPc);
        }
    }
    if (tf) {
        std::fclose(tf);
    }
    std::printf("DOC types=%u exec=%u | UND types=%u exec=%u | KIL exec=%u\n", docTypes, docN,
                undTypes, undN, kilN);

    const auto s = m.snapshot();
    int hgrNz = 0;
    for (int a = 0x2000; a < 0x4000; ++a) {
        if (m.bus().ram()[a]) {
            ++hgrNz;
        }
    }
    std::printf("\n=== SUMMARY ===\n");
    std::printf("final pc=%04X video=%s%s hgrNz=%d edges=%zu\n", s.pc,
                s.text ? "TEXT" : (s.hires ? "HGR" : "LORES"), s.mixed ? "+M" : "", hgrNz,
                m.bus().speaker().edgeCountTotal());
    std::printf("romCalls_in_window=%u firstRom=%04X caller=%04X\n", romCalls, firstRomCall,
                firstRomCaller);
    std::printf("romDataOps_abs_base_D000+=%u firstPc=%04X firstBase=%04X\n", romDataOps,
                firstRomDataPc, firstRomDataEa);
    std::printf("smcWrites=%u firstSmc=%04X\n", smcWrites, firstSmcAddr);
    std::printf("stackAnomalies=%u first=%04X unmatchedRTS=%u first=%04X\n", stackAnomaly,
                firstStackAnomalyPc, unmatchedRts, firstUnmatchedRts);
    if (romCalls == 0 && romDataOps == 0) {
        std::printf("ORIGINAL_APPLE_ROM_NOT_YET_IMPLICATED (no $D000+ fetch/data in window)\n");
    } else if (romDataOps > 0 && romCalls == 0) {
        std::printf("MOTHERBOARD_ROM_DATA_DEPENDENCY (abs operand base $D000+; no code fetch)\n");
    } else {
        std::printf("MOTHERBOARD_ROM_CODE_FETCH (PC in $D000+)\n");
    }
    return 0;
}
