#ifndef PLATFORM_H
#define PLATFORM_H

// current time in microseconds (only care about deltas so can be constant shifted)
int64_t currMicro();
int64_t currNano();

#endif
