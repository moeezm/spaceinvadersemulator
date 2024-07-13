#include <SDL.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>
#include <pthread.h>
#include <assert.h>

#include "emulate8080.h"
#include "machine.h"
#include "platform.h"

#define PIXEL_SIZE_X 2
#define PIXEL_SIZE_Y 3

#define VRAM_START 0x2400

#define FPS 60

struct timeval tv;
int64_t currMicro() {
	gettimeofday(&tv, NULL);
	return (tv.tv_sec)*1000000 + tv.tv_usec;
}

struct timespec ts;
int64_t currNano() {
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec * 1000000000L + ts.tv_nsec;
}

State8080* cpu;
Machine* machine;

const int WINDOW_WIDTH = PIXEL_SIZE_X * SCREEN_WIDTH;
const int WINDOW_HEIGHT = PIXEL_SIZE_Y * SCREEN_HEIGHT;

Uint32* pixels; // actually displayed screen
SDL_Window* window;
SDL_Surface* surface;

void initWindow() {
	printf("%d x %d\n", WINDOW_WIDTH, WINDOW_HEIGHT);
	window = NULL;
	surface = NULL;
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		printf("SDL could not be initialized: %s", SDL_GetError());
		exit(1);
	}
	window = SDL_CreateWindow(
			"SDL Basic",
			SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED,
			WINDOW_WIDTH,
			WINDOW_HEIGHT,
			SDL_WINDOW_SHOWN
	);
	if (window == NULL) {
		printf("Window could not be created: %s", SDL_GetError());
		exit(1);
	}
	surface = SDL_GetWindowSurface(window);
	pixels = surface->pixels;
}

void cleanWindow() {
	SDL_DestroyWindow(window);
	SDL_Quit();
}

bool loadFile(char* filename, int location) {
	FILE* f = fopen(filename, "rb");
	if (f == NULL) {
		printf("Error loading file: %s\n", filename);
		exit(1);
	}
	int filesize;
	fseek(f, 0, SEEK_END);
	filesize = ftell(f);
	fseek(f, 0, SEEK_SET);
	fread(cpu->memory + location, 1, filesize, f);
	fclose(f);
	return true;
}

int main() {
	cpu = initState8080();
	machine = initMachine();
	
	// h,g,f,e at 0x0000, 0x0800, 0x1000, 0x1800
	// load programs
	loadFile("roms/invaders.h", 0x0000);
	loadFile("roms/invaders.g", 0x0800);
	loadFile("roms/invaders.f", 0x1000);
	loadFile("roms/invaders.e", 0x1800);

	initWindow();
	
	SDL_Event e;

	bool running = true;

	const int MICROSECONDS_PER_FRAME = 1000000 / FPS;
	const int64_t NANOSECONDS_PER_FRAME = 1000000000L / FPS;
	const int CLOCKS_PER_FRAME = CLOCK_SPEED / FPS;

	uint64_t lastHalf = currMicro();
	uint64_t lastFull = lastHalf + MICROSECONDS_PER_FRAME / 2;
	int64_t curr;
	int64_t cycles = 0;
	int64_t last = 0;

	u8 vRamCopy[SCREEN_WIDTH][SCREEN_HEIGHT/8];
	memset(vRamCopy, 0, sizeof(vRamCopy));

	if (DISASSEMBLE) initDisassembleFile();
	while (running) {
		while (SDL_PollEvent(&e)) switch (e.type) {
			case SDL_QUIT:
				cpu->on = false;
				running = 0;
				break;
			case SDL_KEYDOWN:
				switch (e.key.keysym.scancode) {
					case 225:
						machineKeyDown(machine, MK_COIN);
						break;
					case 30:
						machineKeyDown(machine, MK_1P_START);
						break;
					case 31:
						machineKeyDown(machine, MK_2P_START);
						break;
					case 4:
						machineKeyDown(machine, MK_1P_LEFT);
						break;
					case 7:
						machineKeyDown(machine, MK_1P_RIGHT);
						break;
					case 26:
						machineKeyDown(machine, MK_1P_SHOT);
						break;
					case 80:
						machineKeyDown(machine, MK_2P_LEFT);
						break;
					case 79:
						machineKeyDown(machine, MK_2P_RIGHT);
						break;
					case 82:
						machineKeyDown(machine, MK_2P_SHOT);
						break;
				}
				break;
			case SDL_KEYUP:
				switch (e.key.keysym.scancode) {
					case 225:
						machineKeyUp(machine, MK_COIN);
						break;
					case 22:
						machineKeyUp(machine, MK_1P_START);
						break;
					case 4:
						machineKeyUp(machine, MK_1P_LEFT);
						break;
					case 7:
						machineKeyUp(machine, MK_1P_RIGHT);
						break;
					case 26:
						machineKeyUp(machine, MK_1P_SHOT);
						break;
					case 81:
						machineKeyUp(machine, MK_2P_START);
						break;
					case 80:
						machineKeyUp(machine, MK_2P_LEFT);
						break;
					case 79:
						machineKeyUp(machine, MK_2P_RIGHT);
						break;
					case 82:
						machineKeyUp(machine, MK_2P_SHOT);
						break;
				}
				break;
		}
		
		if (currMicro() - lastHalf >= MICROSECONDS_PER_FRAME) {
			memcpy(vRamCopy, cpu->memory + VRAM_START, SCREEN_HEIGHT * SCREEN_WIDTH / 8);
			// rotate
			Uint32 pixel;
			for (int i = 0; i < 96; i++) {
				for (int j = 0; j < SCREEN_HEIGHT; j++) {
					pixel = (vRamCopy[i][j/8]>>(j%8) & 1) == 1 ? 0xFFFFFFFF : 0;
					for (int k = 0; k < PIXEL_SIZE_Y; k++) {
						for (int l = 0; l < PIXEL_SIZE_X; l++) {
							pixels[(PIXEL_SIZE_Y*(SCREEN_HEIGHT - 1 - j) + k) * WINDOW_WIDTH + (PIXEL_SIZE_X*i+l)] = pixel;
						}
					}
				}
			}
			SDL_UpdateWindowSurface(window);
			lastHalf = currMicro(); 
			VBlankHalfInterrupt(cpu);
			// lol
			for (int i = 0; i < 1000; i++) nextOp8080(cpu, machine);

		}
		if (currMicro() - lastFull >= MICROSECONDS_PER_FRAME) {
			memcpy(vRamCopy, cpu->memory + VRAM_START, SCREEN_HEIGHT * SCREEN_WIDTH / 8);
			// rotate
			Uint32 pixel;
			for (int i = 96; i < SCREEN_WIDTH; i++) {
				for (int j = 0; j < SCREEN_HEIGHT; j++) {
					pixel = (vRamCopy[i][j/8]>>(j%8) & 1) == 1 ? 0xFFFFFFFF : 0;
					for (int k = 0; k < PIXEL_SIZE_Y; k++) {
						for (int l = 0; l < PIXEL_SIZE_X; l++) {
							pixels[(PIXEL_SIZE_Y*(SCREEN_HEIGHT - 1 - j) + k) * WINDOW_WIDTH + (PIXEL_SIZE_X*i+l)] = pixel;
						}
					}
				}
			}
			SDL_UpdateWindowSurface(window);
			lastFull = currMicro();
			VBlankFullInterrupt(cpu);
			
			while (cycles < CYCLES_PER_BLOCK) {
				cycles += nextOp8080(cpu, machine);
			}
			cycles = 0;
		}
	}
	cleanWindow();
	/*
	// dump memory
	printf("dumping memory...\n");
	FILE* f = fopen("myshika-memdump", "wb");
	fwrite(cpu->memory, 1, MEM_SZ, f);
	fclose(f);
	printf("disassembling...\n");
	*/
	if (DISASSEMBLE) {
		outputDisassembly();
		cleanDisassembleFile();
	}
	return 0;
}
