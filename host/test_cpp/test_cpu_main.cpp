#include "esp_bracket/cpu6502.hpp"
#include "esp_bracket/cpu_harness.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace esp_bracket;

static int g_failures = 0;

static void expect(bool ok, const char *name) {
    if (!ok) {
        std::fprintf(stderr, "FAIL  %s\n", name);
        ++g_failures;
    } else {
        std::printf("PASS  %s\n", name);
    }
}

static void expectEq(uint32_t got, uint32_t want, const char *name) {
    if (got != want) {
        std::fprintf(stderr, "FAIL  %s (got %u want %u)\n", name, got, want);
        ++g_failures;
    } else {
        std::printf("PASS  %s\n", name);
    }
}

static void runProgram(CpuHarness &mem, Cpu6502 &cpu, uint16_t start, uint32_t maxSteps) {
    mem.setResetVector(start);
    cpu.setCallbacks(&mem, CpuHarness::harnessRead, CpuHarness::harnessWrite);
    cpu.reset();
    for (uint32_t i = 0; i < maxSteps; ++i) {
        cpu.step();
    }
}

static void testReset() {
    CpuHarness mem;
    Cpu6502 cpu;
    mem.write8(0x8000, 0xEA); // NOP
    mem.setResetVector(0x8000);
    cpu.setCallbacks(&mem, CpuHarness::harnessRead, CpuHarness::harnessWrite);
    cpu.reset();
    expectEq(cpu.registers().pc, 0x8000, "RESET pc");
}

static void testLdaSta() {
    CpuHarness mem;
    Cpu6502 cpu;
    const uint8_t prog[] = {
        0xA9, 0x42,       // LDA #$42
        0x8D, 0x00, 0x02, // STA $0200
        0xA2, 0x99,       // LDX #$99
        0x8E, 0x01, 0x02, // STX $0201
        0xA0, 0x11,       // LDY #$11
        0x8C, 0x02, 0x02, // STY $0202
        0x00,             // BRK
    };
    mem.load(0x8000, prog, sizeof(prog));
    mem.setIrqVector(0x9000);
    mem.write8(0x9000, 0x40); // RTI
    runProgram(mem, cpu, 0x8000, 8);
    expectEq(mem.read8(0x0200), 0x42, "LDA/STA");
    expectEq(mem.read8(0x0201), 0x99, "LDX/STX");
    expectEq(mem.read8(0x0202), 0x11, "LDY/STY");
}

static void testAdcSbcCmp() {
    CpuHarness mem;
    Cpu6502 cpu;
    const uint8_t prog[] = {
        0x18,             // CLC
        0xA9, 0x10,       // LDA #$10
        0x69, 0x05,       // ADC #$05
        0x8D, 0x10, 0x02, // STA $0210
        0xE9, 0x03, // SBC #$03 (carry still set from ADC? after ADC of 0x15, C clear if no carry
                    // out)
        // After ADC #$05: A=0x15, C=0. SBC without SEC subtracts with borrow.
        // Better sequence:
    };
    // Rewrite carefully:
    // CLC; LDA #10; ADC #5 => A=15 C=0
    // SEC; SBC #3 => A=12
    // CMP #12 => Z=1
    const uint8_t prog2[] = {
        0x18, 0xA9, 0x10, 0x69, 0x05, 0x8D, 0x10, 0x02, // store 0x15
        0x38, 0xE9, 0x03, 0x8D, 0x11, 0x02,             // store 0x12
        0xC9, 0x12,                                     // CMP #$12
        0x00,
    };
    mem.load(0x8000, prog2, sizeof(prog2));
    mem.setIrqVector(0x9000);
    mem.write8(0x9000, 0x40);
    runProgram(mem, cpu, 0x8000, 12);
    expectEq(mem.read8(0x0210), 0x15, "ADC");
    expectEq(mem.read8(0x0211), 0x12, "SBC");
    expect((cpu.registers().status & 0x02) != 0, "CMP Z");
    (void)prog;
}

static void testBranchesJmpJsr() {
    CpuHarness mem;
    Cpu6502 cpu;
    // LDA #0; BEQ +2; LDA #1; (skipped) LDA #2; STA $0200; JMP cont; ...
    const uint8_t prog[] = {
        0xA9, 0x00,       // LDA #0
        0xF0, 0x02,       // BEQ +2
        0xA9, 0xFF,       // skipped
        0xA9, 0x55,       // LDA #55
        0x20, 0x10, 0x80, // JSR $8010
        0x4C, 0x20, 0x80, // JMP $8020
        // $8010:
        0xEA, 0xEA, 0xEA, 0xEA, // pad to 0x8010
        // actually need exact addresses — rebuild with known layout
    };
    // Clean program at $8000:
    // 8000: A9 00        LDA #0
    // 8002: F0 02        BEQ 8006
    // 8004: A9 FF        LDA #FF
    // 8006: A9 77        LDA #77
    // 8008: 20 0E 80     JSR 800E
    // 800B: 8D 00 02     STA $0200
    // 800E: A9 01        LDA #1   (subroutine sets A=1 then RTS — wait that overwrites)
    // Better: JSR increments a counter via INC
    const uint8_t p[] = {
        /*8000*/ 0xA9, 0x00,       // LDA #0
        /*8002*/ 0xF0, 0x02,       // BEQ *+4 -> 8006
        /*8004*/ 0xA9, 0xFF,       //
        /*8006*/ 0x20, 0x0D, 0x80, // JSR $800D
        /*8009*/ 0x8D, 0x00, 0x02, // STA $0200
        /*800C*/ 0x00,             // BRK
        /*800D*/ 0xA9, 0x42,       // LDA #42
        /*800F*/ 0x60,             // RTS
    };
    mem.load(0x8000, p, sizeof(p));
    mem.setIrqVector(0x9000);
    mem.write8(0x9000, 0x40);
    runProgram(mem, cpu, 0x8000, 20);
    expectEq(mem.read8(0x0200), 0x42, "BEQ/JSR/RTS");
    (void)prog;
}

static void testStackPushPop() {
    CpuHarness mem;
    Cpu6502 cpu;
    const uint8_t p[] = {
        0xA9, 0xAB, // LDA #AB
        0x48,       // PHA
        0xA9, 0x00, // LDA #0
        0x68,       // PLA
        0x8D, 0x00, 0x02, 0x00,
    };
    mem.load(0x8000, p, sizeof(p));
    mem.setIrqVector(0x9000);
    mem.write8(0x9000, 0x40);
    runProgram(mem, cpu, 0x8000, 10);
    expectEq(mem.read8(0x0200), 0xAB, "PHA/PLA");
}

static void testZeroPageIndexedIndirect() {
    CpuHarness mem;
    Cpu6502 cpu;
    mem.write8(0x10, 0x00);
    mem.write8(0x11, 0x03); // ($10) -> $0300
    mem.write8(0x0300, 0x5A);
    const uint8_t p[] = {
        0xA2, 0x00,                   // LDX #0
        0xA1, 0x10,                   // LDA ($10,X)
        0x8D, 0x20, 0x02, 0xA0, 0x00, // LDY #0
        0xB1, 0x10,                   // LDA ($10),Y
        0x8D, 0x21, 0x02, 0x00,
    };
    mem.load(0x8000, p, sizeof(p));
    mem.setIrqVector(0x9000);
    mem.write8(0x9000, 0x40);
    runProgram(mem, cpu, 0x8000, 12);
    expectEq(mem.read8(0x0220), 0x5A, "LDA (zp,X)");
    expectEq(mem.read8(0x0221), 0x5A, "LDA (zp),Y");
}

static void testFlags() {
    CpuHarness mem;
    Cpu6502 cpu;
    const uint8_t p[] = {
        0xA9,
        0x00, // LDA #0 -> Z
        0x00,
    };
    mem.load(0x8000, p, sizeof(p));
    mem.setIrqVector(0x9000);
    mem.write8(0x9000, 0x40);
    runProgram(mem, cpu, 0x8000, 4);
    expect((cpu.registers().status & 0x02) != 0, "FLAG Z after LDA #0");
}

static void testNmiIrq() {
    CpuHarness mem;
    Cpu6502 cpu;
    // Main: infinite NOP loop at $8000
    mem.write8(0x8000, 0xEA);
    mem.write8(0x8001, 0x4C);
    mem.write8(0x8002, 0x00);
    mem.write8(0x8003, 0x80); // JMP $8000
    // NMI handler $8100: LDA #1; STA $0200; RTI
    const uint8_t nmi[] = {0xA9, 0x01, 0x8D, 0x00, 0x02, 0x40};
    mem.load(0x8100, nmi, sizeof(nmi));
    mem.setNmiVector(0x8100);
    mem.setIrqVector(0x8200);
    const uint8_t irq[] = {0xA9, 0x02, 0x8D, 0x01, 0x02, 0x40};
    mem.load(0x8200, irq, sizeof(irq));
    mem.setResetVector(0x8000);
    cpu.setCallbacks(&mem, CpuHarness::harnessRead, CpuHarness::harnessWrite);
    cpu.reset();
    cpu.step();
    cpu.nmi();
    for (int i = 0; i < 10; ++i) {
        cpu.step();
    }
    expectEq(mem.read8(0x0200), 0x01, "NMI");
    // Clear I so IRQ works: CLI
    CpuRegisters r = cpu.registers();
    r.status = static_cast<uint8_t>(r.status & ~0x04);
    cpu.setRegisters(r);
    cpu.irq();
    for (int i = 0; i < 10; ++i) {
        cpu.step();
    }
    expectEq(mem.read8(0x0201), 0x02, "IRQ");
}

static void testPageCrossCycles() {
    CpuHarness mem;
    Cpu6502 cpu;
    // LDA abs,X with page cross should add 1 cycle (fake6502 penalty)
    mem.write8(0x20FF, 0); // avoid
    mem.write8(0x2100, 0x99);
    const uint8_t p[] = {
        0xA2, 0x01,       // LDX #1
        0xBD, 0xFF, 0x20, // LDA $20FF,X -> $2100 page cross
        0x00,
    };
    mem.load(0x8000, p, sizeof(p));
    mem.setIrqVector(0x9000);
    mem.write8(0x9000, 0x40);
    mem.setResetVector(0x8000);
    cpu.setCallbacks(&mem, CpuHarness::harnessRead, CpuHarness::harnessWrite);
    cpu.reset();
    cpu.clearCycleCounter();
    uint32_t c1 = cpu.step(); // LDX = 2
    uint32_t c2 = cpu.step(); // LDA abs,X = 4+1
    expectEq(c1, 2, "LDX cycles");
    expectEq(c2, 5, "LDA abs,X page-cross cycles");
    expectEq(cpu.registers().a, 0x99, "LDA abs,X value");
}

/** NMOS undocumented $CB SBX/AXS — required by some Apple II boot paths. */
static void testSbxCb() {
    CpuHarness mem;
    Cpu6502 cpu;
    // A=$F0, X=$0F → A&X=$00; SBX #$01 → X=$FF, C=0
    // A=$80, X=$C0 → A&X=$80; SBX #$10 → X=$70, C=1
    const uint8_t p[] = {
        0xA9, 0x80, // LDA #$80
        0xA2, 0xC0, // LDX #$C0
        0xCB, 0x10, // SBX #$10
        0x00,
    };
    mem.load(0x8000, p, sizeof(p));
    mem.setResetVector(0x8000);
    cpu.setCallbacks(&mem, CpuHarness::harnessRead, CpuHarness::harnessWrite);
    cpu.reset();
    cpu.step(); // LDA
    cpu.step(); // LDX
    cpu.step(); // SBX
    expectEq(cpu.registers().x, 0x70, "SBX X result");
    expect((cpu.registers().status & 0x01) != 0, "SBX carry set");
}

/** NMOS $9F AHX abs,Y and $9C SHY abs,X store masking. */
static void testAhxShy() {
    CpuHarness mem;
    Cpu6502 cpu;
    // SHY $8080,X with Y=0,X=0 → store 0 at $8080
    // AHX $9000,Y with A=$FF,X=$0F,Y=0 → ea=$9000, store FF&0F&($90+1)=FF&0F&91=$01
    mem.write8(0x8080, 0xAA);
    mem.write8(0x9000, 0x00);
    const uint8_t p[] = {
        0xA0, 0x00,       // LDY #0
        0xA2, 0x00,       // LDX #0
        0x9C, 0x80, 0x80, // SHY $8080,X
        0xA9, 0xFF,       // LDA #$FF
        0xA2, 0x0F,       // LDX #$0F
        0x9F, 0x00, 0x90, // AHX $9000,Y
        0x00,
    };
    mem.load(0x8000, p, sizeof(p));
    mem.setResetVector(0x8000);
    cpu.setCallbacks(&mem, CpuHarness::harnessRead, CpuHarness::harnessWrite);
    cpu.reset();
    for (int i = 0; i < 6; ++i) {
        cpu.step();
    }
    expectEq(mem.read8(0x8080), 0x00, "SHY Y=0 stores 0");
    expectEq(mem.read8(0x9000), 0x01, "AHX A&X&(H+1)");
}

int main() {
    testReset();
    testLdaSta();
    testAdcSbcCmp();
    testBranchesJmpJsr();
    testStackPushPop();
    testZeroPageIndexedIndirect();
    testFlags();
    testNmiIrq();
    testPageCrossCycles();
    testSbxCb();
    testAhxShy();
    if (g_failures) {
        std::fprintf(stderr, "\n%d CPU test(s) failed\n", g_failures);
        return 1;
    }
    std::printf("\nAll CPU host tests passed\n");
    return 0;
}
