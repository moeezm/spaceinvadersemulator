# Space Invaders Emulator
Emulates the microprocessor (Intel 8080) and the display for the original arcade machine.

Tried to make it as modular as possible, separating the processor, the "machine" (mostly just input/output ports), and the actual platform that handles displaying stuff

Compile with `` gcc disassemble.c emulate8080.c platform.c machine.c  `sdl2-config --cflags --libs` ``

Also features a disassembly of the program (might not be complete if there are any instructions that didn't get run during my playing).
