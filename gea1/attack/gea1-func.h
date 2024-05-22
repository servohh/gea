#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include "attack-parms.h"

struct lfsr
{
        uint64_t state;
        uint64_t taps;
        unsigned int length;
};

uint8_t getBitAtPos(uint64_t n, unsigned int pos);

uint8_t f(uint8_t x0, uint8_t x1, uint8_t x2, uint8_t x3, uint8_t x4, uint8_t x5, uint8_t x6);

uint64_t clock_lfsr(struct lfsr reg, uint8_t in);

uint64_t clock_s(uint64_t reg, uint8_t in);

void generateOutputB(struct lfsr B, unsigned char* out);

void deriveOutputB(struct lfsr A, struct lfsr C, unsigned char* keystream, unsigned char* out);
