#pragma once

/*
 * Bridge to vendored fake6502 globals (FAKE6502_NOT_STATIC build).
 * Only cpu6502.cpp / fake6502_impl.cpp should include this.
 */
extern "C" {
typedef unsigned short ushort;
typedef unsigned char uint8;
typedef unsigned int uint32;

extern ushort pc;
extern uint8 sp, a, x, y, status;
extern uint32 clockticks6502;
extern uint32 instructions;

void reset6502();
void nmi6502();
void irq6502();
uint32 exec6502(uint32 tickcount);
uint32 step6502();
uint8 read6502(ushort address);
void write6502(ushort address, uint8 value);
}
