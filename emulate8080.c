#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "disassemble.h"
#include "emulate8080.h"
#include "machine.h"
#include "platform.h"

#define DEBUG true 

FILE* outfile; 

State8080* initState8080() {
	State8080* state = malloc(sizeof(State8080));
	memset(state->memory, 0, MEM_SZ);
	memset(state->regs, 0, 8);
	state->psw = 2;
	state->sp = 0;
	state->halted = false;
	state->interrupted = false;
	state->interruptsEnabled = true;
	state->on = true;

	state->interruptbus[0] = 0;
	state->interruptbus[1] = 0;
	state->interruptbus[2] = 0;
	return state;
}

void writeMem(State8080* state, u16 addr, u8 val) {
	if (addr < 0x2000) {
		printf("Cannot write address %X (ROM is addresses < 0x2000) (PC: %X)\n", addr, state->pc);
	}
	else {
		state->memory[addr] = val;
	}
}

void generateInterrupt(State8080* state, u8 opcode, u8 data1, u8 data2) {
	state->interruptbus[0] = opcode;
	state->interruptbus[1] = data1;
	state->interruptbus[2] = data2;
	state->interrupted = true;
}

u16 combine8(u8 lo, u8 hi) {
	return lo + ((u16)hi << 8);
}

void push8(State8080* state, u8 val) {
	writeMem(state, --state->sp, val);
}
void push16(State8080* state, u16 val) {
	// push hi then lo
	push8(state, (u8)(val>>8 & 0xFF));
	push8(state, (u8)(val & 0xFF));
}

u8 pop8(State8080* state) {
	return state->memory[state->sp++];
}
u16 pop16(State8080* state) {
	u16 ans = combine8(state->memory[state->sp], state->memory[state->sp+1]);
	state->sp += 2;
	return ans;
}

// rp = 0b11 can be either SP or PSW (A + status)
u8* dRegHi(State8080* state, u8 rp, bool psw) {
	if (rp < 3) return &(state->regs[rp*2]);
	else if (psw) return &(state->regs[REG_A]);
	else return (u8*)(&(state->sp)) + 1; // little-endian!!!
}
u8* dRegLo(State8080* state, u8 rp, bool psw) {
	if (rp < 3) return &(state->regs[rp*2 + 1]);
	else if (psw) return &(state->psw);
	else return (u8*)(&(state->sp));
}

u8 readReg(State8080* state, int reg) {
	if (reg == REG_M) return state->memory[combine8(readReg(state, REG_L), readReg(state, REG_H))];
	else return state->regs[reg];
}
void writeReg(State8080* state, int reg, u8 val) {
	if (reg == REG_M) writeMem(state, combine8(readReg(state, REG_L), readReg(state, REG_H)), val);
	else state->regs[reg] = val;
}
// b 0 or 1
void setFlag(State8080* state, enum Flag f, u8 b) {
	state->psw = (state->psw & ~(1<<(int)f)) | (b<<(int)f);
}
int getFlag(State8080* state, enum Flag f) {
	return (state->psw >> (int)f) & 1;
}

void setSign(State8080* state, u8 val) {
	setFlag(state, FLAG_S, (val>>7)&1);
}
void setZero(State8080* state, u8 val) {
	setFlag(state, FLAG_Z, val == 0);
}
void setParity(State8080* state, u8 val) {
	u8 ans = 1;
	while (val > 0) {
		ans ^= (val & 1);
		val >>= 1;
	}
	setFlag(state, FLAG_P, ans);
}
void setAC(State8080* state, u16 inp, u16 outp) {
	// if the numbers except for the low nibble are different then a carry or borrow took place
	setFlag(state, FLAG_AC, (inp & ~0xF) != (outp & ~0xF));
}
void setNonCarryFlags(State8080* state, u8 val) {
	setSign(state, val);
	setZero(state, val);
	setParity(state, val);
}

// ALU ops
u8 ALUadd(State8080* state, u8 x1, u8 x2, u8 carry) {
	setFlag(state, FLAG_C, ((u16)x2+(u16)carry > 0xFF-x1));
	setFlag(state, FLAG_AC, (u16)x2 + (u16)carry > 0xF - (x1 & 0x0F));
	u8 y = x1 + x2 + carry;
	setNonCarryFlags(state, y);
	return y;
}

u8 ALUsub(State8080* state, u8 x1, u8 x2, u8 carry) {
	setFlag(state, FLAG_C, (u16)x2+(u16)carry > x1);
	setFlag(state, FLAG_AC, (u16)x2 + (u16)carry > (x1 & 0x0F));
	u8 y = x1 - x2 - carry;
	setNonCarryFlags(state, y);
	return y;
}

u8 ALUand(State8080* state, u8 x1, u8 x2, bool affectAC) {
	setFlag(state, FLAG_C, 0);
	setFlag(state, FLAG_AC, affectAC & ( (x1&(1<<3))>>3 | (x2&(1<<3))>>3 ));
	u8 y = x1 & x2;
	setNonCarryFlags(state, y);
	return y;
}

u8 ALUxor(State8080* state, u8 x1, u8 x2) {
	setFlag(state, FLAG_C, 0);
	setFlag(state, FLAG_AC, 0);
	u8 y = x1 ^ x2;
	setNonCarryFlags(state, y);
	return y;
}

u8 ALUor(State8080* state, u8 x1, u8 x2) {
	setFlag(state, FLAG_C, 0);
	setFlag(state, FLAG_AC, 0);
	u8 y = x1 | x2;
	setNonCarryFlags(state, y);
	return y;
}

void ALUcmp(State8080* state, u8 x1, u8 x2) {
	ALUsub(state, x1, x2, 0);
}

bool evaluateCC(State8080* state, u8 cc) {
	switch (cc) {
		case 0:
			return !getFlag(state, FLAG_Z);
		case 1:
			return getFlag(state, FLAG_Z);
		case 2:
			return !getFlag(state, FLAG_C);
		case 3:
			return getFlag(state, FLAG_C);
		case 4:
			return !getFlag(state, FLAG_P);
		case 5:
			return getFlag(state, FLAG_P);
		case 6:
			return !getFlag(state, FLAG_S);
		case 7:
			return getFlag(state, FLAG_S);
	}
}

// return is number of cycles taken
int emulateOp8080(State8080* state, Machine* machine, u8 op, u8 d1, u8 d2) {
	// pickin out various parts of the opcode
	// reg1 and reg2 are three bits long, where 8 bits are bb-reg1-reg2
	// rp (register pair) is the last two bits of the first nibble
	// f2 is the first two bits of the opcode
	// l4 is the second nibble (last 4 bits)
	u8 reg1, reg2, rp, f2, l4;
	reg1 = (op & 0x38) >> 3;
	reg2 = (op & 0x07);
	rp = (op & 0x30) >> 4;
	f2 = (op & 0xC0) >> 6;
	l4 = (op & 0x0F); 
	bool wasinterrupted = state->interrupted;
	// no parameter ops
	switch (op) {
		case 0x00:
			// NOP
			return 4;
		case 0x07:
		{
			// RLC
			// left shift on A, first bit and carry set to prev 7th bit
			u8 a7 = state->regs[REG_A]>>7 & 1;
			setFlag(state, FLAG_C, a7);
			state->regs[REG_A] <<= 1;
			// set 0th bit to 7th bit
			state->regs[REG_A] = (state->regs[REG_A] & ~1) | a7;
			return 4;
		}
		case 0x0F: 
		{
			// RRC
			// right shift version of RLC
			u8 a0 = (state->regs[REG_A] & 1);
			setFlag(state, FLAG_C, a0);
			state->regs[REG_A] >>= 1;
			state->regs[REG_A] = (state->regs[REG_A] & ~(1<<7)) | a0;
			return 4;
		}
		case 0x17:
		{
			// RAL
			// left shit on A, carry goes to first bit, last bit goes to carry
			u8 cry = getFlag(state, FLAG_C);
			setFlag(state, FLAG_C, state->regs[REG_A]>>7 & 1);
			state->regs[REG_A] <<= 1;
			state->regs[REG_A] = (state->regs[REG_A] & ~1) | cry;
			return 4;
		}
		case 0x1F:
		{
			// RAR
			// right shift version of RAL
			u8 cry = getFlag(state, FLAG_C);
			setFlag(state, FLAG_C, state->regs[REG_A] & 1);
			state->regs[REG_A] >>= 1;
			state->regs[REG_A] = (state->regs[REG_A] & ~(1<<7)) | cry;
			return 4;
		}
		case 0x22:
		{
			// SHLD add
			// puts contents of HL at the memory address
			u16 add = combine8(d1, d2);
			writeMem(state, add++, state->regs[REG_L]);
			writeMem(state, add, state->regs[REG_H]);
			state->pc += 2;
			return 16;
		}
		case 0x27:
		{
			// DAA
			// If low nibble of A is > 9 OR AC == 1 then A += 6
			// then if high nibble of A is > 9 OR Cy == 1 then A += 0x60
			if ((state->regs[REG_A] & 0x0F) > 9 || getFlag(state, FLAG_AC)) state->regs[REG_A] += 6;
			if ((state->regs[REG_A]>>4 & 0x0F) > 9 || getFlag(state, FLAG_C)) state->regs[REG_A] += 0x60;
			return 4;
		}
		case 0x2A:
		{
			// LHLD add
			// load contents at add into HL
			u16 add = combine8(d1, d2);
			state->regs[REG_L] = state->memory[add++];
			state->regs[REG_H] = state->memory[add++];
			state->pc += 2;
			return 16;
		}
		case 0x2F:
		{
			// CMA
			// A = not A
			state->regs[REG_A] = ~state->regs[REG_A];
			return 4;
		}
		case 0x32:
		{
			// STA add
			// put A at add
			writeMem(state, combine8(d1, d2), state->regs[REG_A]);
			state->pc += 2;
			return 13;
		}
		case 0x37:
		{
			// STC
			// Set carry bit
			setFlag(state, FLAG_C, 1);
			return 4;
		}
		case 0x3A:
		{
			// LDA add
			// put contents at add into A
			state->regs[REG_A] = state->memory[combine8(d1, d2)];
			state->pc += 2;
			return 13;
		}
		case 0x3F:
		{
			// CMC
			// Flip carry bit
			setFlag(state, FLAG_C, !getFlag(state, FLAG_C));
			return 4;
		}
		case 0x76:
		{
			// HLT
			// halt until interrupt
			state->halted = true;
			return 7;
		}
		case 0xC3:
		{
			// JMP add
			state->pc = combine8(d1, d2);
			return 10;
		}
		case 0xC9:
		{
			// RET
			// pop off stack into pc
			state->pc = pop16(state);
			return 10;
		}
		case 0xCD:
		{
			// CALL add
			// push pc onto to stack, jump to add
			state->pc += 2;
			push16(state, state->pc);
			state->pc = combine8(d1, d2);
			return 17;
		}
		case 0xD3:
		{
			// OUT port
			// put A in port
			writePort(machine, d1, state->regs[REG_A]);
			state->pc += 1;
			return 10;
		}
		case 0xDB:
		{
			// IN port
			// read from port into A
			state->regs[REG_A] = readPort(machine, d1);
			state->pc += 1;
			return 10;
		}
		case 0xE3:
		{
			// XTHL
			// exchange HL with top of stack
			u16 st = pop16(state);
			push16(state, combine8(state->regs[REG_L], state->regs[REG_H]));
			state->regs[REG_H] = st>>8 & 0xFF;
			state->regs[REG_L] = st & 0xFF;
			return 18;
		}
		case 0xE9:
		{
			// PCHL
			// jump to HL
			state->pc = combine8(state->regs[REG_L], state->regs[REG_H]);
			return 5;
		}
		case 0xEB:
		{
			// XCHG
			// swap HL and DE
			u8 h = state->regs[REG_H];
			u8 l = state->regs[REG_L];
			state->regs[REG_H] = state->regs[REG_D];
			state->regs[REG_L] = state->regs[REG_E];
			state->regs[REG_D] = h;
			state->regs[REG_E] = l;
			return 4;
		}
		case 0xF3:
		{
			// DI
			state->interruptsEnabled = false;
			return 4;
		}
		case 0xF9:
		{
			// SPHL
			// make the stack pointer HL
			state->sp = combine8(state->regs[REG_L], state->regs[REG_H]);
			return 5;
		}
		case 0xFB:
		{
			// EI
			state->interruptsEnabled = true;
			return 4;
		}
	}
	// 16-bit (i.e. double) register operations
	switch (f2) {
		case 0:
			switch (l4) {
				case 0x1:
				{
					// LXI rp, data
					// load 16 bit data into rp
					*dRegLo(state, rp, false) = d1;
					*dRegHi(state, rp, false) = d2;
					state->pc += 2;
					return 10;
				}
				case 0x2:
				{
					// STAX rp
					// put A at address in rp (only BC or DE)
					// other register pair indices correspond to the SHLD and STA operations
					writeMem(state, combine8(*dRegLo(state, rp, false), *dRegHi(state, rp, false)), state->regs[REG_A]);
					return 7;
				}
				case 0x3:
				{
					// INX rp
					// increment rp, doesn't affect flags
					if (++(*dRegLo(state, rp, false)) == 0) (*dRegHi(state, rp, false))++;
					return 5;
				}
				case 0x9:
				{
					// DAD rp
					// HL += rp
					// only carry flag is affected
					int ans = (int)combine8(state->regs[REG_L], state->regs[REG_H]) + (int)combine8(*dRegLo(state, rp, false), *dRegHi(state, rp, false));
					// !! maps x != 0 to 1 and x == 0 to 0
					setFlag(state, FLAG_C, !!(ans/(1<<16)));
					ans &= (1<<16)-1; // mod
					state->regs[REG_L] = ans & 0xFF;
					state->regs[REG_H] = ans>>8 & 0xFF;
					return 10;
				}
				case 0xA:
				{
					// LDAX rp
					// loads A with contents at address stored in RP
					// rp can only be BC or DE, codes for other register pairs correspond to other instructions LHLD and
					// LDA, which have already been covered
					state->regs[REG_A] = state->memory[combine8(*dRegLo(state, rp, false), *dRegHi(state, rp, false))];
					return 7;
				}
				case 0xB:
				{
					// DCX rp
					// decrement rp
					// no flags affected
					if (--(*dRegLo(state, rp, false)) == 0xFF) --(*dRegHi(state, rp, false));
					return 5;
				}
			}
			break;
		case 3:
			switch (l4) {
				case 0x1:
				{
					// POP rp
					// pop from stack to rp
					*dRegLo(state, rp, true) = pop8(state);
					*dRegHi(state, rp, true) = pop8(state);
					return 10;
				}
				case 0x5:
				{
					// PUSH rp
					// push rp onto stack
					push8(state, *dRegHi(state, rp, true));
					push8(state, *dRegLo(state, rp, true));
					return 11;
				}
			}
			break;
	}
	// for single width (8 bit) registers, register 0b110 (0x6) (REG_M) is the value at address stored in HL
	// move register to register
	if (f2 == 1) {
		// MOV r1, r2
		// puts r2 in r1
		// MOV M, M is HLT which was already covered
		writeReg(state, reg1, readReg(state, reg2));
		return (reg1 == REG_M || reg2 == REG_M) ? 7 : 5;
	}
	// single register operations
	if (f2 == 0) {
		switch (reg2) {
			case 0x4:
			{
				// INR r1
				// increment reg1, affects flags except carry
				u16 x = readReg(state, reg1);
				u16 ans = x + 1;
				u8 y = ans & 0xFF; // mod
				setNonCarryFlags(state, y);
				setAC(state, x, ans);
				writeReg(state, reg1, y);
				return 5;
			}
			case 0x5:
			{
				// DCR r1
				// decrement reg1, affects flag except carry
				u16 x = readReg(state, reg1);
				u16 ans = x - 1;
				u8 y = ans & 0xFF;
				setAC(state, x, ans);
				setNonCarryFlags(state, y);
				writeReg(state, reg1, y);
				return 5;
			}
			case 0x6:
			{
				// MVI r1, data
				// puts 8-bit data in reg1
				writeReg(state, reg1, d1);
				state->pc++;
				return 7;
			}
		}
	}
	// Used in interrupts
	if (f2 == 3 && reg2 == 7) {
		// RST n
		// equivalent to CALL at address 8*n
		
		// push
		push16(state, state->pc); 

		state->pc = 8*reg1;
		return 11;
	}
	// CC (conditional) instructions
	if (f2 == 3) {
		switch (reg2) {
			case 0:
			{
				// Rcc
				// if condition is true return
				int shika = 5;
				if (evaluateCC(state, reg1)) {
					state->pc = pop16(state);
					shika = 11; 
				}
				return shika;
			}
			case 2:
			{
				// Jcc ADD
				// reg1(cc) corresponds to a specific condition, if that condition is true then jump to 16-bit add
				state->pc += 2;
				if (evaluateCC(state, reg1)) state->pc = combine8(d1, d2);
				return 10;
			}
			case 4:
			{
				// Ccc ADD
				// if cc is true, call 16-bit address
				state->pc += 2;
				if (evaluateCC(state, reg1)) {
					push16(state, state->pc);
					state->pc = combine8(d1, d2);
				}
				return 11;
			}
		}
	}
	// ALU operations
	// with register
	if (f2 == 2) {
		switch (reg1) {
			case 0:
			{
				writeReg(state, REG_A, ALUadd(state, readReg(state, REG_A), readReg(state, reg2), 0));
				return (reg2 == REG_M ? 7 : 4);
			}
			case 1:
			{
				writeReg(state, REG_A, ALUadd(state, readReg(state, REG_A), readReg(state, reg2), getFlag(state, FLAG_C)));
				return (reg2 == REG_M ? 7 : 4);
			}
			case 2:
			{
				writeReg(state, REG_A, ALUsub(state, readReg(state, REG_A), readReg(state, reg2), 0));
				return (reg2 == REG_M ? 7 : 4);
			}
			case 3:
			{
				writeReg(state, REG_A, ALUsub(state, readReg(state, REG_A), readReg(state, reg2), getFlag(state, FLAG_C)));
				return (reg2 == REG_M ? 7 : 4);
			}
			case 4:
			{
				writeReg(state, REG_A, ALUand(state, readReg(state, REG_A), readReg(state, reg2), true));
				return (reg2 == REG_M ? 7 : 4);
			}
			case 5:
			{
				writeReg(state, REG_A, ALUxor(state, readReg(state, REG_A), readReg(state, reg2)));
				return (reg2 == REG_M ? 7 : 4);
			}
			case 6:
			{
				writeReg(state, REG_A, ALUor(state, readReg(state, REG_A), readReg(state, reg2)));
				return (reg2 == REG_M ? 7 : 4);
			}
			case 7:
			{
				ALUcmp(state, readReg(state, REG_A), readReg(state, reg2));
				return (reg2 == REG_M ? 7 : 4);
			}
		}
	}
	// immediate value ALU
	if (f2 == 3 && reg2 == 6) {
		state->pc++;
		switch (reg1) {
			case 0:
			{
				writeReg(state, REG_A, ALUadd(state, readReg(state, REG_A), d1, 0));
				return 7;
			}
			case 1:
			{
				writeReg(state, REG_A, ALUadd(state, readReg(state, REG_A), d1, getFlag(state, FLAG_C)));
				return 7;
			}
			case 2:
			{
				writeReg(state, REG_A, ALUsub(state, readReg(state, REG_A), d1, 0));
				return 7;
			}
			case 3:
			{
				writeReg(state, REG_A, ALUsub(state, readReg(state, REG_A), d1, getFlag(state, FLAG_C)));
				return 7;
			}
			case 4:
			{
				writeReg(state, REG_A, ALUand(state, readReg(state, REG_A), d1, false));
				return 7;
			}
			case 5:
			{
				writeReg(state, REG_A, ALUxor(state, readReg(state, REG_A), d1));
				return 7;
			}
			case 6:
			{
				writeReg(state, REG_A, ALUor(state, readReg(state, REG_A), d1));
				return 7;
			}
			case 7:
			{
				ALUcmp(state, readReg(state, REG_A), d1);
				return 7;
			}
		}
	}
	state->on = false;
	return 10000;
}

int nextOp8080(State8080* state, Machine* machine) {
	u8 op, d1, d2;
	op = d1 = d2 = 0;
	u16 oldpc = state->pc;
	bool wasinterrupted = false;
	if (!state->interrupted || !state->interruptsEnabled) {
		if (state->halted) return 1;
		op = state->memory[state->pc];
		// d1 and d2 are data
		if (op < MEM_SZ-1) d1 = state->memory[state->pc+1];
		if (op < MEM_SZ-2) d2 = state->memory[state->pc+2];
		state->pc++;
	}
	else {
		wasinterrupted = true;
		op = state->interruptbus[0];
		d1 = state->interruptbus[1];
		d2 = state->interruptbus[2];
	}
	//if (DEBUG) printf("PC: %X, OP: %X, D1: %X, D2: %X\n", state->pc - (int)(!wasinterrupted), op, d1, d2);
	if (DEBUG) {
		fprintf(outfile, "INT?:%d\t", wasinterrupted);
		disassemble8080(outfile, op, d1, d2, oldpc);
		fprintf(outfile, "\t%02X\t%04X\t%02X", state->psw, state->sp, state->memory[state->sp]);
		fprintf(outfile, "\n");
	}
	
	int ans = emulateOp8080(state, machine, op, d1, d2);
	if (ans == 10000) printf("bad instruction at %X\n", oldpc);
	// clear interruptbus since interrupts should not
	// be queued
	state->interruptbus[0] = 0;
	state->interruptbus[1] = 0;
	state->interruptbus[2] = 0;
	state->interrupted = false;
	return ans;
}

const int CYCLES_PER_MILLISECOND = CLOCK_SPEED / 1000;

void run8080(State8080* state, Machine* machine) {
	outfile = fopen("pgdump", "wb");
	//outfile = stdout; 
	uint64_t cycles;
	int64_t last;
	while (state->on) {
		cycles = 0;
		last = currMicro();
		while (cycles < CYCLES_PER_MILLISECOND) {
			cycles += nextOp8080(state, machine);
		}
		uint64_t delta = currMicro() - last;
		if (delta < 1000) {
			usleep(1000 - delta);
		}
		else {
			printf("Cycle took too long: %ld microseconds\n", delta);
		}
	}
	if (outfile != stdout) fclose(outfile);
}
