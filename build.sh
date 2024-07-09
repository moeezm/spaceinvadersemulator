#!/bin/bash
gcc disassemble.c emulate8080.c machine.c platform.c `sdl2-config --cflags --libs`
