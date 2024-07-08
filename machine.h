#ifndef MACHINE_H
#define MACHINE_H
// the monitor is *actually* 256 x 224 but it's been rotated 90deg
#define SCREEN_WIDTH 224
#define SCREEN_HEIGHT 256 
#include <stdint.h>


typedef uint8_t u8;
typedef uint16_t u16;

enum MKey {
	MK_2P_START, MK_1P_START, MK_1P_SHOT, MK_1P_LEFT, MK_1P_RIGHT, MK_2P_SHOT, MK_2P_LEFT, MK_2P_RIGHT
};
typedef struct SpaceInvadersMachine {
	// read ports
	u8 rports[4];

	// write ports
	u8 wport2;
	u16 wport4; // shift register
} Machine;

#include "emulate8080.h"

Machine* initMachine();
void VBlankHalfInterrupt(State8080* state);
void VBlankFullInterrupt(State8080* state);
u8 readPort(Machine* mach, u8 port);
void writePort(Machine* mach, u8 port, u8 val);
void machineKeyToggle(Machine* mach, enum MKey key);

// just a utility function to make it easier for platform writers
// the monitor in Space Invaders is rotated 90 degrees counterclockwise
void rotateScreen(u8 input[SCREEN_WIDTH][SCREEN_HEIGHT/8], int output[SCREEN_HEIGHT][SCREEN_WIDTH]);

#endif 
