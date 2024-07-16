#ifndef SIMULATE8080_H
#define SIMULATE8080_H

#include <stdint.h>
#include <stdbool.h>

#define MEM_SZ (1<<16)
#define CLOCK_SPEED 2000000

#define DEBUG false 
#define DISASSEMBLE false
#define SPACE_INVADERS_MEM_SAFETY false

typedef uint8_t u8;
typedef uint16_t u16;

enum Flag {
	FLAG_C=0,FLAG_P=2,FLAG_AC=4,FLAG_Z=6,FLAG_S=7
};
enum Reg {
	REG_B, REG_C, REG_D, REG_E, REG_H, REG_L, REG_M, REG_A
};
typedef struct State8080 {
	u8 regs[8]; // B, C, D, E, H, L, M, A
	u8 psw; // status register
	u16 pc;
	u16 sp;
	volatile u8 interruptbus[3]; // op, optional data1 and data2
	bool interrupted;
	bool halted;
	u8 memory[MEM_SZ+2];
	bool interruptsEnabled;
	volatile bool on;
} State8080;

#include "machine.h"

State8080* initState8080();
static inline void writeMem(State8080* state, u16 addr, u8 val);
void generateInterrupt(State8080* state, u8 opcode, u8 data1, u8 data2);
int emulateOp8080(State8080* state, Machine* machine, u8 op, u8 d1, u8 d2);
int nextOp8080(State8080* state, Machine* machine);
void run8080(State8080* state, Machine* machine);

void initPcLogFile();
void initDisassembleFile();
void cleanPcLogFile();
void cleanDisassembleFile();
void outputDisassembly();
#endif
