#ifndef ATTACK_PARMS
#define ATTACK_PARMS

#define CONST_L 72
#define ENTRY_SIZE (CONST_L + 24) / 8

#define TAPS_A 0b1011000111011001000110111011101
#define TAPS_B 0b10100010000011110000001110001111
#define TAPS_C 0b001001000101111101100111000010101

#define SHIFT_A 0
#define SHIFT_B 16
#define SHIFT_C 32

#define SIZE_V 1l << 8
#define SIZE_TAC 1l << 24
#define SIZE_UB 1l << 32

#define NUM_THREADS 16

#endif
