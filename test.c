#include <stdio.h>
#include <stdint.h>

typedef uint8_t u8;
typedef uint16_t u16;
int main() {
	u16 x = 0xCCFF;
	printf("%X, %X\n", *((u8*)(&x)), *((u8*)(&x) + 1));
}
