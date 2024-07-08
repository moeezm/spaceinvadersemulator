#!/bin/bash
gcc -g emulate8080.c machine.c platform.c `sdl2-config --cflags --libs`
