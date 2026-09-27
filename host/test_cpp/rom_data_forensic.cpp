/**
 * HOST-ONLY ROM-region access forensics (data vs code vs write).
 *
 * Usage:
 *   rom_data_forensic --ram-init zero|random [--post N] [--out-dir DIR]
 *
 * Writes aggregated maps under gitignored local/apple2/forensics/ only.
 * Does not modify emulator semantics beyond optional bus-spy callbacks.
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

struct SpyCtx {
    Apple2Bus *bus = nullptr;
    uint16_t fetchPc = 0;
    uint8_t fetchOp = 0;
    uint64_t cycle = 0;
    uint8_t a = 0, x = 0, y = 0, p = 0;
    bool active = false;

    // Aggregated: per page-byte index 0..0x2FFF for $D000-$FFFF
    static constexpr int kRomBytes = 0x3000;
    uint32_t dataReadCount[kRomBytes]{};
    uint32_t codeFetchCount[kRomBytes]{};
    uint32_t writeAttemptCount[kRomBytes]{};
    uint16_t firstDataReaderPc[kRomBytes]{};
    uint8_t firstDataValue[kRomBytes]{};
    uint8_t seenDataValue[kRomBytes]{}; // OR of observed values (diagnostic)
    bool hasData[kRomBytes]{};

    uint32_t totalDataReads = 0;
    uint32_t totalCodeFetches = 0;
    uint32_t totalWriteAttempts = 0;
    uint16_t firstDataPc = 0;
    uint16_t firstDataEa = 0;
    uint8_t firstDataVal = 0;
    uint64_t firstDataCycle = 0;
    uint16_t firstCodePc = 0;
};

bool isFetchStream(const SpyCtx *s, uint16_t addr) {
    // Instruction stream: PC .. PC+2 covers all NMOS ops (1–3 bytes).
    const uint16_t pc = s->fetchPc;
    return addr == pc || addr == static_cast<uint16_t>(pc + 1) ||
           addr == static_cast<uint16_t>(pc + 2);
}

uint8_t spyRead(void *ctx, uint16_t address) {
    auto *s = static_cast<SpyCtx *>(ctx);
    const uint8_t v = Apple2Bus::busRead(s->bus, address);
    if (!s->active || address < 0xD000u) {
        return v;
    }
    const int off = static_cast<int>(address - 0xD000u);

    if (s->fetchPc >= 0xD000u && isFetchStream(s, address)) {
        ++s->codeFetchCount[off];
        ++s->totalCodeFetches;
        if (!s->firstCodePc) {
            s->firstCodePc = s->fetchPc;
        }
        return v;
    }

    ++s->dataReadCount[off];
    ++s->totalDataReads;
    if (!s->hasData[off]) {
        s->hasData[off] = true;
        s->firstDataReaderPc[off] = s->fetchPc;
        s->firstDataValue[off] = v;
        s->seenDataValue[off] = v;
    } else {
        s->seenDataValue[off] = static_cast<uint8_t>(s->seenDataValue[off] | v);
    }
    if (!s->firstDataPc) {
        s->firstDataPc = s->fetchPc;
        s->firstDataEa = address;
        s->firstDataVal = v;
        s->firstDataCycle = s->cycle;
    }
    return v;
}

void spyWrite(void *ctx, uint16_t address, uint8_t value) {
    auto *s = static_cast<SpyCtx *>(ctx);
    if (s->active && address >= 0xD000u) {
        const int off = static_cast<int>(address - 0xD000u);
        ++s->writeAttemptCount[off];
        ++s->totalWriteAttempts;
    }
    Apple2Bus::busWrite(s->bus, address, value);
    (void)value;
}

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

// DOS 3.3 DSK: track t sector s → offset (t*16+s)*256
const uint8_t *dskSector(const std::vector<uint8_t> &dsk, int track, int sector) {
    const size_t off = static_cast<size_t>((track * 16 + sector) * 256);
    if (off + 256 > dsk.size()) {
        return nullptr;
    }
    return dsk.data() + off;
}

} // namespace

int main(int argc, char **argv) {
    const char *romPath = "local/apple2/roms/appleiigo.rom";
    const char *diskPath = "local/apple2/disks/Galaxian.dsk";
    const char *outDir = "local/apple2/forensics/rom_data";
    RamInitMode ramInit = RamInitMode::Random;
    int postInstr = 2000;

    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--rom") && i + 1 < argc) {
            romPath = argv[++i];
        } else if (!std::strcmp(argv[i], "--disk") && i + 1 < argc) {
            diskPath = argv[++i];
        } else if (!std::strcmp(argv[i], "--out-dir") && i + 1 < argc) {
            outDir = argv[++i];
        } else if (!std::strcmp(argv[i], "--post") && i + 1 < argc) {
            postInstr = static_cast<int>(std::strtol(argv[++i], nullptr, 10));
        } else if (!std::strcmp(argv[i], "--ram-init") && i + 1 < argc) {
            ++i;
            if (!std::strcmp(argv[i], "zero")) {
                ramInit = RamInitMode::Zero;
            } else if (!std::strcmp(argv[i], "ones")) {
                ramInit = RamInitMode::Ones;
            } else if (!std::strcmp(argv[i], "random")) {
                ramInit = RamInitMode::Random;
            }
        }
    }

#ifdef _WIN32
    std::string mk = std::string("mkdir \"") + outDir + "\" 2>nul";
    std::system(mk.c_str());
#else
    std::system((std::string("mkdir -p \"") + outDir + "\"").c_str());
#endif

    std::vector<uint8_t> rom, disk;
    if (!loadFile(romPath, &rom) || !loadFile(diskPath, &disk)) {
        std::fprintf(stderr, "FAIL load\n");
        return 2;
    }
    char romHex[65], diskHex[65];
    Sha256::hashHex(rom.data(), rom.size(), romHex);
    Sha256::hashHex(disk.data(), disk.size(), diskHex);
    std::printf("ROM sha256=%s size=%zu\n", romHex, rom.size());
    std::printf("DISK sha256=%s size=%zu\n", diskHex, disk.size());
    std::printf("RAM_INIT=%u\n", static_cast<unsigned>(ramInit));

    // AppleIIGo $D5xx metadata only
    int d5nz = 0;
    for (int i = 0; i < 256; ++i) {
        if (rom[0x500 + i]) {
            ++d5nz;
        }
    }
    std::printf("AppleIIGo D500-D5FF nonzero_bytes=%d (of 256)\n", d5nz);
    std::printf("AppleIIGo D549=%02X D5E7=%02X\n", rom[0x549], rom[0x5E7]);

    HostAppleIIMachine m;
    RomIdentity id{};
    if (m.loadRomIdentified(rom.data(), rom.size(), &id) != RomError::Ok) {
        return 3;
    }
    m.setSlot6RomMode(Slot6RomMode::CleanRoom);
    if (m.mountDisk(DriveId::Drive1, diskPath) != MediaResult::Ok) {
        return 4;
    }
    m.powerOn(ramInit);
    {
        CpuRegisters r = m.cpu().registers();
        r.pc = 0xC600;
        m.cpu().setRegisters(r);
    }

    SpyCtx spy{};
    spy.bus = &m.bus();
    // Keep default bus callbacks through Slot-6 boot; install spy after handoff.

    bool handoff = false;
    uint8_t b200AtHandoff[16]{};
    uint8_t b200AtFirstFb[16]{};
    bool sawFb = false;
    uint32_t smcToB200 = 0;
    uint8_t prevB2[256];
    bool prevInit = false;
    uint8_t yAtFirstFb = 0;
    uint16_t eaAtFirstFb = 0;

    // Boot to handoff (native bus callbacks)
    for (uint64_t i = 0; i < 3000000; ++i) {
        const uint16_t pc = m.cpu().registers().pc;
        if (!handoff && pc == 0x0842) {
            const uint8_t *ram = m.bus().ram();
            if (ram[0x0842] == 0x4C && ram[0x0843] == 0x00 && ram[0x0844] == 0xB1) {
                handoff = true;
                const CpuRegisters r = m.cpu().registers();
                std::printf("HANDOFF cycle=%llu A=%02X X=%02X Y=%02X SP=%02X P=%02X\n",
                            (unsigned long long)m.cpu().cycles(), r.a, r.x, r.y, r.sp, r.status);
                std::memcpy(b200AtHandoff, ram + 0xB200, 16);
                std::printf("RAM@$B200.. at handoff:");
                for (int k = 0; k < 16; ++k) {
                    std::printf(" %02X", b200AtHandoff[k]);
                }
                std::printf("\n");
                std::memcpy(prevB2, ram + 0xB200, 256);
                prevInit = true;
                break;
            }
        }
        if (!handoff && pc >= 0xB100 && pc <= 0xB1FF && m.cpu().cycles() > 10000) {
            handoff = true;
            const uint8_t *ram = m.bus().ram();
            const CpuRegisters r = m.cpu().registers();
            std::printf("HANDOFF_FALLBACK pc=%04X cycle=%llu A=%02X X=%02X Y=%02X SP=%02X P=%02X\n",
                        pc, (unsigned long long)m.cpu().cycles(), r.a, r.x, r.y, r.sp, r.status);
            std::memcpy(b200AtHandoff, ram + 0xB200, 16);
            std::printf("RAM@$B200.. at handoff:");
            for (int k = 0; k < 16; ++k) {
                std::printf(" %02X", b200AtHandoff[k]);
            }
            std::printf("\n");
            std::memcpy(prevB2, ram + 0xB200, 256);
            prevInit = true;
            break;
        }
        const bool fine = (pc >= 0x0800 && pc < 0x0900) || (pc >= 0xC600 && pc < 0xC700);
        m.runCycles(fine ? 1 : 64);
    }
    if (!handoff) {
        std::fprintf(stderr, "FAIL no handoff pc=%04X\n", m.cpu().registers().pc);
        return 5;
    }

    m.cpu().setCallbacks(&spy, spyRead, spyWrite);
    spy.active = true;

    // Operand integrity vs DSK
    bool foundOnDisk = false;
    int diskTrack = -1, diskSect = -1, diskOff = -1;
    for (int s = 0; s < 16 && !foundOnDisk; ++s) {
        const uint8_t *sec = dskSector(disk, 0, s);
        if (!sec) {
            continue;
        }
        for (int o = 0; o < 254; ++o) {
            if (sec[o] == 0xFB && sec[o + 1] == 0x49 && sec[o + 2] == 0xD5) {
                foundOnDisk = true;
                diskTrack = 0;
                diskSect = s;
                diskOff = o;
                break;
            }
        }
    }
    std::printf("DISK_OPERAND FB 49 D5 %s T0S%02X off=%d\n", foundOnDisk ? "FOUND" : "NOT_FOUND",
                diskSect, diskOff);
    std::printf("OPERAND_AT_HANDOFF B200=%02X B201=%02X B202=%02X (%s)\n", b200AtHandoff[0],
                b200AtHandoff[1], b200AtHandoff[2],
                (b200AtHandoff[0] == 0xFB && b200AtHandoff[1] == 0x49 && b200AtHandoff[2] == 0xD5)
                    ? "ISB_$D549_Y"
                    : "OTHER");

    // Compare loaded $B200 page to the DSK sector that matches handoff content.
    // Boot0 maps sectors by DOS order into $B0+; $B2 page index = 2.
    {
        const uint8_t *ram = m.bus().ram();
        int matchSect = -1;
        for (int s = 0; s < 16; ++s) {
            const uint8_t *sec = dskSector(disk, 0, s);
            if (!sec) {
                continue;
            }
            if (std::memcmp(sec, ram + 0xB200, 256) == 0) {
                matchSect = s;
                break;
            }
            // Allow known patches only outside $B2
        }
        // Soft match: ignore if patches elsewhere; check $B200-$B20F vs any T0 sector
        if (matchSect < 0) {
            for (int s = 0; s < 16; ++s) {
                const uint8_t *sec = dskSector(disk, 0, s);
                if (sec && std::memcmp(sec, ram + 0xB200, 16) == 0) {
                    matchSect = s;
                    break;
                }
            }
        }
        std::printf("RAM_$B200_PAGE vs T0 sector match_s=%d\n", matchSect);
        (void)diskTrack;
    }

    for (int n = 0; n < postInstr; ++n) {
        CpuRegisters r = m.cpu().registers();
        const uint8_t *ram = m.bus().ram();
        spy.fetchPc = r.pc;
        spy.fetchOp = ram[r.pc];
        spy.cycle = m.cpu().cycles();
        spy.a = r.a;
        spy.x = r.x;
        spy.y = r.y;
        spy.p = r.status;

        if (spy.fetchOp == 0xFB && !sawFb) {
            sawFb = true;
            yAtFirstFb = r.y;
            eaAtFirstFb = static_cast<uint16_t>(0xD549 + r.y);
            std::memcpy(b200AtFirstFb, ram + 0xB200, 16);
            std::printf("FIRST_ISB_FB cycle=%llu pc=%04X A=%02X X=%02X Y=%02X P=%02X ea=%04X "
                        "rom_byte=%02X\n",
                        (unsigned long long)spy.cycle, r.pc, r.a, r.x, r.y, r.status, eaAtFirstFb,
                        m.bus().peek(eaAtFirstFb));
            std::printf("RAM@$B200 at first FB:");
            for (int k = 0; k < 16; ++k) {
                std::printf(" %02X", b200AtFirstFb[k]);
            }
            std::printf("\n");
            const bool intact = std::memcmp(b200AtHandoff, b200AtFirstFb, 3) == 0 &&
                                b200AtFirstFb[0] == 0xFB && b200AtFirstFb[1] == 0x49 &&
                                b200AtFirstFb[2] == 0xD5;
            std::printf("OPERAND_INTEGRITY %s smc_to_B2xx_before_FB=%u\n",
                        intact ? "INTACT_FROM_HANDOFF" : "MODIFIED", smcToB200);
        }

        m.cpu().step();
        serviceCleanRoomCardRequests(m.bus().ram(), m.diskII());

        if (prevInit && !sawFb) {
            for (int i = 0; i < 256; ++i) {
                if (ram[0xB200 + i] != prevB2[i]) {
                    ++smcToB200;
                    if (smcToB200 <= 4) {
                        std::printf("SMC $%04X %02X->%02X before_FB from_pc=%04X\n", 0xB200 + i,
                                    prevB2[i], ram[0xB200 + i], spy.fetchPc);
                    }
                }
            }
            std::memcpy(prevB2, ram + 0xB200, 256);
        }
    }

    // Write aggregate map (addresses with data reads only)
    char path[512];
    std::snprintf(path, sizeof(path), "%s/rom_data_map.txt", outDir);
    if (FILE *f = std::fopen(path, "w")) {
        std::fprintf(f, "rom_sha256=%s\ndisk_sha256=%s\nram_init=%u\n", romHex, diskHex,
                     static_cast<unsigned>(ramInit));
        std::fprintf(f, "total_data_reads=%u total_code_fetches=%u total_write_attempts=%u\n",
                     spy.totalDataReads, spy.totalCodeFetches, spy.totalWriteAttempts);
        std::fprintf(f, "first_data pc=%04X ea=%04X val=%02X cycle=%llu\n", spy.firstDataPc,
                     spy.firstDataEa, spy.firstDataVal, (unsigned long long)spy.firstDataCycle);
        std::fprintf(f, "first_code_fetch_pc=%04X\n", spy.firstCodePc);
        std::fprintf(f, "# addr count first_reader_pc first_val\n");
        for (int off = 0; off < SpyCtx::kRomBytes; ++off) {
            if (spy.dataReadCount[off] == 0) {
                continue;
            }
            std::fprintf(f, "%04X %u %04X %02X\n", 0xD000 + off, spy.dataReadCount[off],
                         spy.firstDataReaderPc[off], spy.firstDataValue[off]);
        }
        std::fclose(f);
    }

    std::printf("\n=== ROM ACCESS SUMMARY ===\n");
    std::printf("data_reads=%u code_fetches=%u write_attempts=%u\n", spy.totalDataReads,
                spy.totalCodeFetches, spy.totalWriteAttempts);
    std::printf("first_ROM_DATA pc=%04X ea=%04X val=%02X cycle=%llu\n", spy.firstDataPc,
                spy.firstDataEa, spy.firstDataVal, (unsigned long long)spy.firstDataCycle);
    std::printf("first_ROM_CODE=%04X\n", spy.firstCodePc);
    if (sawFb) {
        std::printf("Y_at_first_FB=%02X EA_range_hint=D549+Y -> %04X\n", yAtFirstFb, eaAtFirstFb);
    }

    // Unique addresses in $D5xx
    int d5addrs = 0;
    uint32_t d5reads = 0;
    for (int i = 0; i < 256; ++i) {
        if (spy.dataReadCount[0x500 + i]) {
            ++d5addrs;
            d5reads += spy.dataReadCount[0x500 + i];
        }
    }
    std::printf("D5xx data: unique_addrs=%d total_reads=%u\n", d5addrs, d5reads);

    if (spy.totalDataReads > 0 && spy.totalCodeFetches == 0) {
        std::printf("CLASSIFICATION=APPLEIIGO_MISSING_REQUIRED_DATA\n");
        std::printf("GATE=BLOCKED_REQUIRES_USER_APPLE_II_PLUS_ROM\n");
    } else if (spy.totalDataReads == 0) {
        std::printf("CLASSIFICATION=NO_ROM_DATA_IN_WINDOW (Stage A / Zero trap)\n");
    }

    return 0;
}
