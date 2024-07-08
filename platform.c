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

#define PIXEL_SIZE 2
#define GRID_THICK 0
#define _convertX(x) (((GRID_THICK) + (PIXEL_SIZE))*(x) + (GRID_THICK))
#define _convertY(y) (((GRID_THICK) + (PIXEL_SIZE))*(y) + (GRID_THICK))

#define VRAM_START 0x2400

#define FPS 60

int64_t currMicro() {
	struct timeval tv;
	gettimeofday(&tv, NULL);
	int64_t t = (tv.tv_sec)*1000000 + tv.tv_usec;
	return t;
}

State8080* cpu;
Machine* machine;

const int WINDOW_WIDTH = _convertX(SCREEN_WIDTH);
const int WINDOW_HEIGHT = _convertY(SCREEN_HEIGHT);

Uint32* pixels; // actually displayed screen
SDL_Window* window;
SDL_Surface* surface;

void setPixel(int x, int y, Uint32 color) {
	assert(color == 1 || color == 0);
	if (color == 1) {
		color = 0xFFFFFFFF;
	}
	int start = _convertY(y)*WINDOW_WIDTH + _convertX(x);
	for (int i = 0; i < PIXEL_SIZE; i++) {
		for (int j = 0; j < PIXEL_SIZE; j++) {
			pixels[start + i*WINDOW_WIDTH + j] = color;
		}
	}
}

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

void* cpuThreadWrapper(void* arg) {
	run8080(cpu, machine);
	pthread_exit(NULL);
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

	pthread_t thread;
	pthread_create(&thread, NULL, cpuThreadWrapper, NULL);
	const uint64_t MICROSECONDS_PER_FRAME = 1000000 / FPS;
	uint64_t lastHalf = 0;
	uint64_t lastFull = MICROSECONDS_PER_FRAME / 2;
	uint64_t curr;

	u8 vRamCopy[SCREEN_WIDTH][SCREEN_HEIGHT/8];
	int rotScreen[SCREEN_HEIGHT][SCREEN_WIDTH];
	while (running) {
		while (SDL_PollEvent(&e)) switch (e.type) {
			case SDL_QUIT:
				cpu->on = false;
				running = 0;
				break;
		}
		curr = currMicro();
		memcpy(vRamCopy, cpu->memory + VRAM_START, SCREEN_HEIGHT * SCREEN_WIDTH / 8);
		rotateScreen(vRamCopy, rotScreen);
		if (curr - lastHalf >= MICROSECONDS_PER_FRAME) {
			// draw top half of frame (first 96 lines)
			for (int i = 0; i < 96; i++) {
				for (int j = 0; j < SCREEN_WIDTH; j++) {
					setPixel(j, i, rotScreen[i][j]);
				}
			}
			SDL_UpdateWindowSurface(window);
			lastHalf = currMicro(); 
			VBlankHalfInterrupt(cpu);
		}
		if (curr - lastFull >= MICROSECONDS_PER_FRAME) {
			// draw bottom half of frame
			for (int i = 96; i < SCREEN_HEIGHT; i++) {
				for (int j = 0; j < SCREEN_WIDTH; j++) {
					setPixel(j, i, rotScreen[i][j]);
				}
			}
			SDL_UpdateWindowSurface(window);
			lastFull = currMicro();
			VBlankFullInterrupt(cpu);
		}
	}
	cleanWindow();
	return 0;
}
