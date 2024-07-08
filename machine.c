#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <assert.h>
#include "machine.h"
#include "emulate8080.h"

void VBlankHalfInterrupt(State8080* state) {
	generateInterrupt(state, 0xCF, 0, 0);
}

void VBlankFullInterrupt(State8080* state) {
	generateInterrupt(state, 0xD7, 0, 0);
}

u8 readPort(Machine* mach, u8 port) {
	return mach->rports[port];
}

void writePort(Machine* mach, u8 port, u8 val) {
	switch (port) {
		case 2: mach->wport2 = val & 0x7; break; // first 3 bits
		case 4: mach->wport4 = (mach->wport4>>8) | ((u16)val << 8); break;
	}
	// update rport3
	mach->rports[3] = (mach->wport4 >> (8 - mach->wport2)) & 0xFF;
}

Machine* initMachine() {
	Machine* m;
	memset(m->rports, 0, 4);
	m->wport2 = 0;
	m->wport4 = 0;
	return m;
}

void machineKeyToggle(Machine* mach, enum MKey key) {
	switch (key) {
		case MK_2P_START:
			mach->rports[1] ^= (1<<1);
			break;
		case MK_1P_START:
			mach->rports[1] ^= (1<<2);
			break;
		case MK_1P_SHOT:
			mach->rports[1] ^= (1<<4);
			break;
		case MK_1P_LEFT:
			mach->rports[1] ^= (1<<5);
			break;
		case MK_1P_RIGHT:
			mach->rports[1] ^= (1<<6);
			break;
		case MK_2P_SHOT:
			mach->rports[2] ^= (1<<4);
			break;
		case MK_2P_LEFT:
			mach->rports[2] ^= (1<<5);
			break;
		case MK_2P_RIGHT:
			mach->rports[2] ^= (1<<6);
			break;
	}
}

//void rotateScreen(u8 input[SCREEN_WIDTH][SCREEN_HEIGHT/8], int output[SCREEN_WIDTH][SCREEN_HEIGHT]) {
void rotateScreen(u8 input[SCREEN_WIDTH][SCREEN_HEIGHT/8], int output[SCREEN_HEIGHT][SCREEN_WIDTH]) {
	// VRAM is bytes [0x2400, 0x4000)
	// each byte is 8 pixels
	// we'll just convert this to an array of 0s and 1s
	// that is rotated 90 degrees counterclockwise
	
	// to rotate 90deg CCW, transpose then flip along the new
	// height dimension
	// so (i, j) -> (SCREEN_HEIGHT-j, i)
	
	int k, b;
	for (int i = 0; i < SCREEN_WIDTH; i++) {
		for (int j = 0; j < SCREEN_HEIGHT; j++) {
			k = j/8; b = j%8;
			output[SCREEN_HEIGHT - j][i] = (input[i][k]>>(8-b))&1;
		}
	}
}
