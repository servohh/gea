#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include "attack-parms.h"
#include "gea1-func.h"

// GEA-1 functions.
uint8_t getBitAtPos(uint64_t n, unsigned int pos)
{
        return (uint8_t)((n >> pos) & 1);
}

uint8_t f(uint8_t x0, uint8_t x1, uint8_t x2, uint8_t x3, uint8_t x4, uint8_t x5, uint8_t x6) 
{
        return x0&x2&x5&x6 ^ x0&x3&x5&x6 ^ x0&x1&x5&x6 ^ x1&x2&x5&x6 ^ x0&x2&x3&x6 ^ x1&x3&x4&x6 ^ x1&x3&x5&x6 ^ x0&x2&x4 ^ x0&x2&x3 ^ x0&x1&x3 ^ x0&x2&x6 ^ x0&x1&x4 ^ x0&x1&x6 ^ x1&x2&x6 ^ x2&x5&x6 ^ x0&x3&x5 ^ x1&x4&x6 ^ x1&x2&x5 ^ x0&x3 ^ x0&x5 ^ x1&x3 ^ x1&x5 ^ x1&x6 ^ x0&x2 ^ x1 ^ x2&x3 ^ x2&x5 ^ x2&x6 ^ x4&x5 ^ x5&x6 ^ x2 ^ x3 ^ x5;
}

uint64_t clock_lfsr(struct lfsr reg, uint8_t in) 
{
        uint8_t output = reg.state & 1;
        output ^= in;
        reg.state >>= 1;
        if (output) {
                reg.state ^= reg.taps;
        }
        return reg.state;
}

uint64_t clock_s(uint64_t reg, uint8_t in) 
{
        uint8_t output = reg & 1;
        uint8_t fOut = f(getBitAtPos(reg, 3), getBitAtPos(reg, 12), getBitAtPos(reg, 22), getBitAtPos(reg, 38), getBitAtPos(reg, 42), getBitAtPos(reg, 55), getBitAtPos(reg, 63));
        reg >>= 1;
        reg ^= ((uint64_t)(in ^ fOut ^ output)) << 63;
        return reg;
}

// Generate output from lfsr B. Used when creating the table
void generateOutputB(struct lfsr B, unsigned char* out)
{
        unsigned char output;
        for (int i = 0;i < (CONST_L / 8);i++) {
                output = 0;
                for (int j = 0;j < 8;j++) {
			uint8_t f_B = f(getBitAtPos(B.state, 12), getBitAtPos(B.state, 27), getBitAtPos(B.state, 0), getBitAtPos(B.state, 1), getBitAtPos(B.state, 29), getBitAtPos(B.state, 21), getBitAtPos(B.state, 5));

                        output >>= 1;
                        output |= f_B << 7;

                        B.state = clock_lfsr(B, 0);
                }
                out[i] = output;
        }
}

// Derive output from lfsr B, using outputs from A and C as well as keystream. Used in the online step
void deriveOutputB(struct lfsr A, struct lfsr C, unsigned char* keystream, unsigned char* out)
{
        unsigned char output;
        for (int i = 0;i < (CONST_L / 8);i++) {
                output = 0;
                for (int j = 0;j < 8;j++) {
                        uint8_t f_A = f(getBitAtPos(A.state, 22), getBitAtPos(A.state, 0), getBitAtPos(A.state, 13), getBitAtPos(A.state, 21), getBitAtPos(A.state, 25), getBitAtPos(A.state, 2), getBitAtPos(A.state, 7));
			uint8_t f_C = f(getBitAtPos(C.state, 10), getBitAtPos(C.state, 30), getBitAtPos(C.state, 32), getBitAtPos(C.state, 3), getBitAtPos(C.state, 19), getBitAtPos(C.state, 0), getBitAtPos(C.state, 4));
                        
			output >>= 1;
                        output |= (f_A ^ f_C) << 7;

                        A.state = clock_lfsr(A, 0);
			C.state = clock_lfsr(C, 0);
                }
                out[i] = keystream[i] ^ output;
        }
}
