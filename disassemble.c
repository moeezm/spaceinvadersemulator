#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>

#define pp(...) fprintf(outfile, __VA_ARGS__)

#include "disassemble.h"

char* regs[8] = {"B", "C", "D", "E", "H", "L", "M", "A"};
char* dregs[4] = {"BC", "DE", "HL", "PSW"};
char* ccs[8] = {"NZ", "Z", "NC", "C", "PO", "PE", "P", "N"};
char* alureg[8] = {"ADD", "ADC", "SUB", "SBB", "ANA", "XRA", "ORA", "CMP"};
char* aludat[8] = {"ADI", "ACI", "SUI", "SBI", "ANI", "XRI", "ORI", "CPI"};

static u16 combine8(u8 lo, u8 hi) {
	return lo + ((u16)hi << 8);
}

int disassemble8080(FILE* outfile, u8 op, u8 d1, u8 d2, u16 pc) {
	int opsz = 1;
	// pickin out various parts of the opcode
	// reg1 and reg2 are three bits long, where 8 bits are bb-reg1-reg2
	// rp (register pair) is the last two bits of the first nibble
	// f2 is the first two bits of the opcode
	// l4 is the second nibble (last 4 bits)
	pp("%04X\t", pc);
	u8 reg1, reg2, rp, f2, l4;
	reg1 = (op & 0x38) >> 3;
	reg2 = (op & 0x07);
	rp = (op & 0x30) >> 4;
	f2 = (op & 0xC0) >> 6;
	l4 = (op & 0x0F); 
	// no parameter ops
	switch (op) {
		case 0x00:
			// NOP
			pp("NOP \t\t\t\t");
			goto end;
		case 0x07:
		{
			// RLC
			pp("RLC \t\t\t\t");
			goto end; 
		}
		case 0x0F: 
		{
			// RRC
			pp("RRC \t\t\t\t");
			goto end; 
		}
		case 0x17:
		{
			// RAL
			pp("RAL \t\t\t\t");
			goto end; 
		}
		case 0x1F:
		{
			// RAR
			pp("RAR \t\t\t\t");
			goto end; 
		}
		case 0x22:
		{
			// SHLD add
			pp("SHLD\t%04X\t\t", combine8(d1, d2));
			opsz+= 2;
			goto end; 
		}
		case 0x27:
		{
			// DAA
			pp("DAA \t\t\t\t");
			goto end; 
		}
		case 0x2A:
		{
			// LHLD add
			pp("LHLD\t%04X\t\t", combine8(d1, d2)); 
			opsz+=2; 
			goto end; 
		}
		case 0x2F:
		{
			// CMA
			pp("CMA \t\t\t\t");
			goto end; 
		}
		case 0x32:
		{
			// STA add
			pp("STA \t%04X\t\t", combine8(d1, d2)); 
			opsz += 2; 
			goto end; 
		}
		case 0x37:
		{
			// STC
			pp("STC \t\t\t\t");
			goto end; 
		}
		case 0x3A:
		{
			// LDA add
			pp("LDA \t%04X\t\t", combine8(d1, d2));
			opsz += 2; 
			goto end; 
		}
		case 0x3F:
		{
			// CMC
			pp("CMC \t\t\t\t"); 
			goto end; 
		}
		case 0x76:
		{
			// HLT
			pp("HLT \t\t\t\t");
			goto end; 
		}
		case 0xC3:
		{
			// JMP add
			pp("JMP \t%04X\t\t", combine8(d1, d2));
			opsz += 2; 
			goto end; 
		}
		case 0xC9:
		{
			// RET
			pp("RET \t\t\t\t");
			goto end; 
		}
		case 0xCD:
		{
			// CALL add
			pp("CALL\t%04X\t\t", combine8(d1, d2));
			opsz += 2; 
			goto end; 
		}
		case 0xD3:
		{
			// OUT port
			pp("OUT \t%02X  \t\t", d1); 
			opsz++; 
			goto end; 
		}
		case 0xDB:
		{
			// IN port
			pp("IN  \t%02X  \t\t", d1);
			opsz++; 
			goto end; 
		}
		case 0xE3:
		{
			// XTHL
			pp("XTHL\t\t\t\t");
			goto end; 
		}
		case 0xE9:
		{
			// PCHL
			pp("PCHL\t\t\t\t");
			goto end; 
		}
		case 0xEB:
		{
			// XCHG
			pp("XCHG\t\t\t\t"); 
			goto end; 
		}
		case 0xF3:
		{
			// DI
			pp("DI  \t\t\t\t"); 
			goto end; 
		}
		case 0xF9:
		{
			// SPHL
			pp("SPHL\t\t\t\t"); 
			goto end; 
		}
		case 0xFB:
		{
			// EI
			pp("EI  \t\t\t\t"); 
			goto end; 
		}
	}
	// 16-bit (i.e. double) register operations
	switch (f2) {
		case 0:
			switch (l4) {
				case 0x1:
				{
					// LXI rp, data
					pp("LXI \t%-4s\t%02X  ", dregs[rp], combine8(d1, d2));
					opsz += 2;
					goto end; 
				}
				case 0x2:
				{
					// STAX rp
					pp("STAX \t%-4s\t\t", dregs[rp]);
					goto end; 
				}
				case 0x3:
				{
					// INX rp
					pp("INX \t%-4s\t\t", dregs[rp]); 
					goto end; 
				}
				case 0x9:
				{
					// DAD rp
					pp("DAD \t%-4s\t\t", dregs[rp]);
					goto end; 
				}
				case 0xA:
				{
					// LDAX rp
					pp("LDAX \t%-4s\t\t", dregs[rp]);
					goto end; 
				}
				case 0xB:
				{
					// DCX rp
					pp("DCX \t%-4s\t\t", dregs[rp]);
					goto end; 
				}
			}
			break;
		case 3:
			switch (l4) {
				case 0x1:
				{
					// POP rp
					pp("POP \t%-4s\t\t", dregs[rp]);
					goto end; 
				}
				case 0x5:
				{
					// PUSH rp
					pp("PUSH\t%-4s\t\t", dregs[rp]);
					goto end; 
				}
			}
			break;
	}
	// for single width (8 bit) registers, register 0b110 (0x6) (REG_M) is the value at address stored in HL
	// move register to register
	if (f2 == 1) {
		// MOV r1, r2
		pp("MOV \t%-3s,\t%-4s", regs[reg1], regs[reg1]); 
		goto end; 
	}
	// single register operations
	if (f2 == 0) {
		switch (reg2) {
			case 0x4:
			{
				// INR r1
				pp("INR \t%-4s\t\t", regs[reg1]);
				goto end; 
			}
			case 0x5:
			{
				// DCR r1
				pp("DCR \t%-4s\t\t", regs[reg1]);
				goto end; 
			}
			case 0x6:
			{
				// MVI r1, data
				pp("MVI \t%-4s,\t%02X  ", regs[reg1], d1);
				opsz++; 
				goto end; 
			}
		}
	}
	// Reset(?) instruction
	// used for interrupts 
	if (f2 == 3 && reg2 == 7) {
		// RST n
		pp("RST \t%X   \t\t", reg1);
		goto end; 
	}
	// CC (conditional) instructions
	if (f2 == 3) {
		switch (reg2) {
			case 0:
			{
				// Rcc
				pp("R%-3s\t\t\t\t", ccs[reg1]);
				goto end; 
			}
			case 2:
			{
				// Jcc ADD
				pp("J%-3s\t%04X\t\t", ccs[reg1], combine8(d1, d2));
				opsz += 2;
				goto end; 
			}
			case 4:
			{
				// Ccc ADD
				pp("C%-3s\t%04X\t\t", ccs[reg1], combine8(d1, d2));
				opsz += 2;
				goto end;
			}
		}
	}
	// ALU operations
	// with register
	if (f2 == 2) {
		pp("%-4s\t%-4s\t\t", alureg[reg1], regs[reg2]);
		goto end;
	}
	// immediate value ALU
	if (f2 == 3 && reg2 == 6) {
		pp("%-4s\t%02X  \t\t", aludat[reg1], d1);
		opsz += 1;
		goto end;
	}
	pp("!%03X\t\t\t\t", op);
end:
	return opsz;
}
