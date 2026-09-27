/**
 * HOST-ONLY SingleStepTests/65x02 runner for NMOS undocumented opcodes.
 *
 * Reads packed .sstb produced by host/tools/sst_pack.mjs (local/gitignored).
 * Does not embed multi-MB JSON in the repo.
 *
 * Usage:
 *   sst_undoc_runner path/to/9c.sstb [--max-fail-samples N]
 */
#include "esp_bracket/cpu6502.hpp"
#include "esp_bracket/cpu_harness.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace esp_bracket;

namespace {

constexpr uint32_t kMagic = 0x42545353u; // 'SSTB' LE

struct RamPair {
    uint16_t addr = 0;
    uint8_t val = 0;
};

struct Vector {
    uint16_t initPc = 0;
    uint8_t initS = 0, initA = 0, initX = 0, initY = 0, initP = 0;
    uint16_t finalPc = 0;
    uint8_t finalS = 0, finalA = 0, finalX = 0, finalY = 0, finalP = 0;
    std::vector<RamPair> initRam;
    std::vector<RamPair> finalRam;
};

struct PackedFile {
    uint8_t opcode = 0;
    std::vector<Vector> vectors;
};

bool readExact(std::ifstream &in, void *dst, size_t n) {
    in.read(reinterpret_cast<char *>(dst), static_cast<std::streamsize>(n));
    return static_cast<size_t>(in.gcount()) == n;
}

uint16_t readU16(std::ifstream &in, bool *ok) {
    uint8_t b[2];
    if (!readExact(in, b, 2)) {
        *ok = false;
        return 0;
    }
    return static_cast<uint16_t>(b[0] | (static_cast<uint16_t>(b[1]) << 8));
}

uint32_t readU32(std::ifstream &in, bool *ok) {
    uint8_t b[4];
    if (!readExact(in, b, 4)) {
        *ok = false;
        return 0;
    }
    return static_cast<uint32_t>(b[0] | (static_cast<uint32_t>(b[1]) << 8) |
                                 (static_cast<uint32_t>(b[2]) << 16) |
                                 (static_cast<uint32_t>(b[3]) << 24));
}

uint8_t readU8(std::ifstream &in, bool *ok) {
    uint8_t b = 0;
    if (!readExact(in, &b, 1)) {
        *ok = false;
    }
    return b;
}

bool loadSstb(const char *path, PackedFile *out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    bool ok = true;
    const uint32_t magic = readU32(in, &ok);
    if (!ok || magic != kMagic) {
        std::fprintf(stderr, "bad magic in %s\n", path);
        return false;
    }
    out->opcode = readU8(in, &ok);
    (void)readU8(in, &ok);
    (void)readU8(in, &ok);
    (void)readU8(in, &ok);
    const uint32_t count = readU32(in, &ok);
    if (!ok) {
        return false;
    }
    out->vectors.resize(count);
    for (uint32_t i = 0; i < count; ++i) {
        Vector &v = out->vectors[i];
        v.initPc = readU16(in, &ok);
        v.initS = readU8(in, &ok);
        v.initA = readU8(in, &ok);
        v.initX = readU8(in, &ok);
        v.initY = readU8(in, &ok);
        v.initP = readU8(in, &ok);
        v.finalPc = readU16(in, &ok);
        v.finalS = readU8(in, &ok);
        v.finalA = readU8(in, &ok);
        v.finalX = readU8(in, &ok);
        v.finalY = readU8(in, &ok);
        v.finalP = readU8(in, &ok);
        const uint16_t ni = readU16(in, &ok);
        v.initRam.resize(ni);
        for (uint16_t r = 0; r < ni; ++r) {
            v.initRam[r].addr = readU16(in, &ok);
            v.initRam[r].val = readU8(in, &ok);
        }
        const uint16_t nf = readU16(in, &ok);
        v.finalRam.resize(nf);
        for (uint16_t r = 0; r < nf; ++r) {
            v.finalRam[r].addr = readU16(in, &ok);
            v.finalRam[r].val = readU8(in, &ok);
        }
        if (!ok) {
            std::fprintf(stderr, "truncated at vector %u\n", i);
            return false;
        }
    }
    return true;
}

struct FailSample {
    uint32_t index = 0;
    Vector v;
    CpuRegisters got{};
    std::vector<RamPair> gotRam;
    std::string reason;
};

bool runVector(const Vector &v, FailSample *failOut) {
    CpuHarness mem;
    Cpu6502 cpu;
    mem.clear(0x00);
    for (const RamPair &p : v.initRam) {
        mem.write8(p.addr, p.val);
    }

    cpu.setCallbacks(&mem, CpuHarness::harnessRead, CpuHarness::harnessWrite);
    CpuRegisters r{};
    r.pc = v.initPc;
    r.sp = v.initS;
    r.a = v.initA;
    r.x = v.initX;
    r.y = v.initY;
    r.status = v.initP;
    cpu.setRegisters(r);
    cpu.step();

    const CpuRegisters got = cpu.registers();
    auto mismatch = [&](const char *why) {
        if (!failOut) {
            return false;
        }
        failOut->v = v;
        failOut->got = got;
        failOut->reason = why;
        failOut->gotRam.clear();
        for (const RamPair &p : v.finalRam) {
            failOut->gotRam.push_back({p.addr, mem.read8(p.addr)});
        }
        return false;
    };

    if (got.pc != v.finalPc) {
        return mismatch("pc");
    }
    if (got.sp != v.finalS) {
        return mismatch("s");
    }
    if (got.a != v.finalA) {
        return mismatch("a");
    }
    if (got.x != v.finalX) {
        return mismatch("x");
    }
    if (got.y != v.finalY) {
        return mismatch("y");
    }
    if (got.status != v.finalP) {
        return mismatch("p");
    }
    for (const RamPair &p : v.finalRam) {
        if (mem.read8(p.addr) != p.val) {
            return mismatch("ram");
        }
    }
    return true;
}

void printSample(const FailSample &f) {
    const Vector &v = f.v;
    const uint8_t op = [&]() {
        for (const RamPair &p : v.initRam) {
            if (p.addr == v.initPc) {
                return p.val;
            }
        }
        return static_cast<uint8_t>(0);
    }();
    uint8_t b1 = 0, b2 = 0;
    for (const RamPair &p : v.initRam) {
        if (p.addr == static_cast<uint16_t>(v.initPc + 1)) {
            b1 = p.val;
        }
        if (p.addr == static_cast<uint16_t>(v.initPc + 2)) {
            b2 = p.val;
        }
    }
    std::printf("FAIL#%u reason=%s op=%02X operands=%02X %02X\n", f.index, f.reason.c_str(), op, b1,
                b2);
    std::printf("  init  pc=%04X A=%02X X=%02X Y=%02X SP=%02X P=%02X\n", v.initPc, v.initA, v.initX,
                v.initY, v.initS, v.initP);
    std::printf("  expect pc=%04X A=%02X X=%02X Y=%02X SP=%02X P=%02X\n", v.finalPc, v.finalA,
                v.finalX, v.finalY, v.finalS, v.finalP);
    std::printf("  actual pc=%04X A=%02X X=%02X Y=%02X SP=%02X P=%02X\n", f.got.pc, f.got.a,
                f.got.x, f.got.y, f.got.sp, f.got.status);
    std::printf("  init_ram:");
    for (const RamPair &p : v.initRam) {
        std::printf(" [%04X]=%02X", p.addr, p.val);
    }
    std::printf("\n  expect_ram:");
    for (const RamPair &p : v.finalRam) {
        std::printf(" [%04X]=%02X", p.addr, p.val);
    }
    std::printf("\n  actual_ram:");
    for (const RamPair &p : f.gotRam) {
        std::printf(" [%04X]=%02X", p.addr, p.val);
    }
    std::printf("\n");
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: sst_undoc_runner FILE.sstb [--max-fail-samples N]\n");
        return 2;
    }
    const char *path = argv[1];
    int maxSamples = 8;
    for (int i = 2; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--max-fail-samples") && i + 1 < argc) {
            maxSamples = static_cast<int>(std::strtol(argv[++i], nullptr, 10));
        }
    }

    PackedFile file;
    if (!loadSstb(path, &file)) {
        std::fprintf(stderr, "FAIL load %s\n", path);
        return 3;
    }

    uint32_t pass = 0, fail = 0;
    std::vector<FailSample> samples;
    for (uint32_t i = 0; i < file.vectors.size(); ++i) {
        FailSample fs;
        fs.index = i;
        if (runVector(file.vectors[i], &fs)) {
            ++pass;
        } else {
            ++fail;
            if (static_cast<int>(samples.size()) < maxSamples) {
                samples.push_back(fs);
            }
        }
    }

    std::printf("SST opcode=$%02X file=%s\n", file.opcode, path);
    std::printf("vectors_available=%zu vectors_executed=%zu PASS=%u FAIL=%u\n", file.vectors.size(),
                file.vectors.size(), pass, fail);
    for (const FailSample &s : samples) {
        printSample(s);
    }
    return fail ? 1 : 0;
}
