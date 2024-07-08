#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <stdint.h>

#define pp(...) fprintf(outfile, __VA_ARGS__)

typedef uint8_t uint8;
typedef uint16_t uint16;

char regs[8] = {'B', 'C', 'D', 'E', 'H', 'L', 'M', 'A'};
char* dregs[4] = {"BC", "DE", "HL", "PSW"};
char* ccs[8] = {"NZ", "Z", "NC", "C", "PO", "PE", "P", "N"};
char* alureg[8] = {"ADD", "ADC", "SUB", "SBB", "ANA", "XRA", "ORA", "CMP"};
char* aludat[8] = {"ADI", "ACI", "SUI", "SBI", "ANI", "XRI", "ORI", "CPI"};

int disassemble(uint8* buffer, FILE* outfile, int pc) {
	int opsz = 1;
	uint8 op = buffer[pc];
	// pickin out various parts of the opcode
	// reg1 and reg2 are three bits long, where 8 bits are bb-reg1-reg2
	// rp is the last two bits of the first nibble
	// f2 is the first two bits of the opcode
	// l4 is the second nibble (last 4 bits)

	uint8 reg1 = (op & 0x38) >> 3;
	uint8 reg2 = (op & 0x7);
	uint8 rp = (op & 0x30) >> 4;
	uint8 f2 = (op & 0xC0) >> 6;
	uint8 l4 = (op & 0xF);
	// commands wth no parameters
	switch (op) {
		case 0x0: pp("NOP"); break;
		case 0x7: pp("RLC"); break;
		case 0xF: pp("RRC"); break;
		case 0x17: pp("RAL"); break;
		case 0x1F: pp("RAR"); break;
		case 0x27: pp("DAA"); break;
		case 0x2F: pp("CMA"); break;
		case 0x37: pp("STC"); break;
		case 0x3F: pp("CMC"); break;
		case 0x76: pp("HLT"); break;
		case 0xC3: pp("JMP %02X%02X", buffer[pc+2], buffer[pc+1]); opsz += 2; break;
		case 0xC9: pp("RET"); break;
		case 0xCD: pp("CALL %02X%02X", buffer[pc+2], buffer[pc+1]); opsz += 2; break;
		case 0xD3: pp("OUT %02X", buffer[pc+1]); opsz += 1; break;
		case 0xDB: pp("IN %02X", buffer[pc+1]); opsz += 1; break;
		case 0xE3: pp("XTHL"); break;
		case 0xE9: pp("PCHL"); break;
		case 0xEB: pp("XCHG"); break;
		case 0xF3: pp("DI"); break;
		case 0xF9: pp("SPHL"); break;
		case 0xFB: pp("EI"); break;
	} 
	// double-register ops
	switch (f2) {
		case 0x0:
			switch (l4) {
				case 0x1: pp("LXI %s,%02X%02X", dregs[rp], buffer[pc+2], buffer[pc+1]); opsz += 2; break;
				case 0x2: pp("STAX %s", dregs[rp]); break;
				case 0x3: pp("INX %s", dregs[rp]); break;
				case 0x9: pp("DAD %s", dregs[rp]); break;
				case 0xA: pp("LDAX %s", dregs[rp]); break;
				case 0xB: pp("DCX %s", dregs[rp]); break;
			}
			break;
		case 0x3:
			switch (l4) {
				case 0x1: pp("POP %s", dregs[rp]); break;
				case 0x5: pp("PUSH %s", dregs[rp]); break;
			}
			break;
	}
	// 1 register ops
	if (f2 == 0) {
		switch (reg2) {
			case 0x4: pp("INR %02X", regs[reg1]); break;
			case 0x5: pp("DCR %02X", regs[reg1]); break;
			case 0x6: pp("MVI %02X,%02X", regs[reg1], buffer[pc+1]); opsz++; break;
		}
	}
	// MOV
	if (f2 == 1) {
		pp("MOV %c,%c", regs[reg1], regs[reg2]);
	}
	// status flag (CC) ops
	if (f2 == 3) {
		switch (reg2) {
			case 0x0: pp("R%s", ccs[reg1]); break;
			case 0x2: pp("J%s %02X%02X", ccs[reg1], buffer[pc+2], buffer[pc+1]); opsz += 2; break;
			case 0x4: pp("C%s %02X%02X", ccs[reg1], buffer[pc+2], buffer[pc+1]); opsz += 2; break;
		}
	}
	// RST
	if (f2 == 3 && reg2 == 7) {
		pp("RST %X", reg1);
	}

	// w/ data ALU ops
	if (f2 == 3 && reg2 == 6) {
		pp("%s %02X", aludat[reg1], buffer[pc+1]);
		opsz++;
	}
	// w/ other reg ALU ops
	if (f2 == 2) {
		pp("%s %c", alureg[reg1], regs[reg1]);
	}
	pp("\n");
	return opsz;
}

int main(int argc, char *argv[]) {
	if (argc != 3) {
		printf("Usage: ./disassemble INFILE OUTFILE\n");
		return 1;
	}
	FILE *infile = fopen(argv[1], "r");
	if (infile == NULL) {
		perror("Error opening input file");
		return 1;
	}
	FILE *outfile = fopen(argv[2], "w");
	if (outfile == NULL) {
		perror("Error opening output file");
		return 1;
	}
	fseek(infile, 0, SEEK_END);
	int insize = ftell(infile);
	fseek(infile, 0, SEEK_SET);
	printf("Read: %d bytes\n", insize);
	uint8 *buffer = malloc(insize);
	if (buffer == NULL) {
		perror("Error allocating memory for file");
		return 1;
	}

	fread(buffer, 1, insize, infile);
	int pc = 0;
	while (pc < insize) {
		pp("%04X\t", pc);
		pc += disassemble(buffer, outfile, pc);
	}

	free(buffer);
	fclose(infile);
	fclose(outfile);
	return 0;
}
