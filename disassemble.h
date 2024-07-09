#ifndef DISASSEMBLE_H
#define DISASSEMBLE_H

#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;

int disassemble8080(FILE* outfile, u8 op, u8 d1, u8 d2, u16 pc);

#endif
