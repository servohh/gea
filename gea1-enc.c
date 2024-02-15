#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>

typedef uint64_t word;
typedef uint8_t bit;

// Returns an unsigned 64-bit int consisting of all 0's except a 1 in the position denoted by pos, 
// e.g.: bitMask(0) returns 0b1, bitMask(5) returns 0b100000.
// Assumes 0 <= pos <= 63
word bitMask(unsigned int pos) 
{
	word bit = 0b1;
	bit <<= pos;
	return bit;
}

// Returns 0 or 1 depending on the value of the bit at position pos in n.
// e.g.: testBit(0b1000, 3) returns 1, testBit(0b1000, 2) returns 0.
bit testBit(word n, unsigned int pos)
{
	if ((n & bitMask(pos))) {
		return 1;
	} else {
		return 0;
	}
}

// GEA-1 nonlinear function
bit f(bit x0, bit x1, bit x2, bit x3, bit x4, bit x5, bit x6) 
{
	return x0&x2&x5&x6 ^ x0&x3&x5&x6 ^ x0&x1&x5&x6 ^ x1&x2&x5&x6 ^ x0&x2&x3&x6 ^ x1&x3&x4&x6 ^ x1&x3&x5&x6 ^ x0&x2&x4 ^ x0&x2&x3 ^ x0&x1&x3 ^ x0&x2&x6 ^ x0&x1&x4 ^ x0&x1&x6 ^ x1&x2&x6 ^ x2&x5&x6 ^ x0&x3&x5 ^ x1&x4&x6 ^ x1&x2&x5 ^ x0&x3 ^ x0&x5 ^ x1&x3 ^ x1&x5 ^ x1&x6 ^ x0&x2 ^ x1 ^ x2&x3 ^ x2&x5 ^ x2&x6 ^ x4&x5 ^ x5&x6 ^ x2 ^ x3 ^ x5;
}

// Galois LFSR struct
struct lfsr
{
	word state;
	word taps;
	unsigned int length;
};

// Galois LFSR clock function
word clock_lfsr(struct lfsr reg, bit in) 
{
	bit output = testBit(reg.state, 0);
	output ^= in;
	reg.state >>= 1;
	if (output) {
		reg.state ^= reg.taps;
	}
	return reg.state;
}

// Clock function for the non-linear feedback register S
word clock_S(word reg, bit in) 
{
	bit output = testBit(reg, 0);
	bit fOut = f(testBit(reg, 3), testBit(reg, 12), testBit(reg, 22), testBit(reg, 38), testBit(reg, 42), testBit(reg, 55), testBit(reg, 63));
	word mask = in ^ (fOut ^ output);
	mask <<= 63;
	reg >>= 1;
	reg ^= mask;
	return reg;
}

// fopen with error checking
FILE* fopenErr(char* filename, char* mode)
{
	FILE* file = fopen(filename, mode);
	if (file == NULL) {
		fprintf(stderr, "gea1-enc: error opening file %s\n", filename);
	}
	return file;
}

int readBytes(FILE* file, char* buf, int size)
{
	char c;
	int i = 0;
	while (i < size) {
		c = fgetc(file);
		if (c == EOF) break;
		buf[i] = c;
		i++;
	}
	return i;
}

void writeBytes(FILE* file, char* buf, int size)
{
	char c;
	for (int i = 0;i < size;i++) {
		c = buf[i];
                fputc(c, file);
        }
}

int main(int argc, char* argv[])
{
	srand(time(NULL));

	// Managing arguments. FILE and KEY are required - IV and DIR are optional
	if (argc < 3) {
		printf("gea1-enc\nUsage: %s FILE [IV] [DIR] KEY\nTakes 64-bit key from file KEY and encrypts FILE using GEA-1 algorithm. If IV and DIR are not given, they will be randomly chosen.\n", argv[0]);
		return 0;
	}
	
	bool chosenDir = false;
	int chosenIV = 0;
	bit dir = 0;
	
	if (argc == 5) {
		chosenDir = true;
		switch (*argv[2]) {
			case '1':
				dir = 1;
			case '0':
				chosenIV = 3;
				break;
			default:
				chosenIV = 2;
		}
		if (chosenIV == 2) {
			switch (*argv[3]) {
				case '1':
					dir = 1;
				case '0':
					break;
				default:
					printf("gea1-enc: dir must be either 0 or 1\n");
					return 1;
			}
		}
	} else if (argc == 4) {
		switch (*argv[2]) {
			case '1':
				dir = 1;
			case '0':
				chosenDir = true;
				break;
			default:
				chosenIV = 2;
		}
	} 

	char* inFileName = argv[1];
	char* keyFileName = argv[argc - 1];
	char* ivFileName = argv[chosenIV]; // if chosenIV == 0, the value of this will not matter

	// Initializing variables
	word key = 0;
	char* keyBuf = malloc(8*sizeof(char));
	FILE* keyFile = fopenErr(keyFileName, "r");
	if (keyFile == NULL) return 1;
	readBytes(keyFile, keyBuf, 8);
	fclose(keyFile);
	for (int i = 0;i < 8;i++) {
		key <<= 8;
		key ^= ((word)keyBuf[i]) & 0xFF;
	}
	free(keyBuf);

	char* plaintext = malloc(1601*sizeof(char));
	int ptSize;
	FILE* inFile = fopenErr(inFileName, "r");
	if (inFile == NULL) return 1;
	ptSize = readBytes(inFile, plaintext, 1601);
	fclose(inFile);

	if (chosenDir == false) dir = rand() % 2;
	printf("dir is %u\n", dir);

	uint32_t iv = 0;

	if (chosenIV == 0) {
		iv = rand();
		char* ivOutBuf = malloc(4*sizeof(char));
		char* ivOutFileName = malloc(132*sizeof(char));
		sprintf(ivOutFileName, "%.128s-iv", inFileName);
		FILE* ivOutFile = fopenErr(ivOutFileName, "w");
		if (ivOutFile == NULL) return 1;
		for (int i = 0;i < 4;i++) {
			ivOutBuf[i] = (iv >> (24 - (i*8))) & 0xFF;
		}
		writeBytes(ivOutFile, ivOutBuf, 4);
		fclose(ivOutFile);
		free(ivOutBuf);
		printf("Wrote generated IV to %s\n", ivOutFileName);
	} else {
		char* ivBuf = malloc(4*sizeof(char));
		FILE* ivFile = fopen(ivFileName, "r");
		if (ivFile == NULL) return 1;
		readBytes(ivFile, ivBuf, 4);
		fclose(ivFile);
		for (int i = 0;i < 4;i++) {
			iv <<= 8;
			iv ^= ((uint32_t)ivBuf[i]) & 0xFF;
		}
		printf("Successfully read IV from %s\n", ivFileName);
	}

	struct lfsr A = {0, 0b1011101110110001001101110001101, 31};
	struct lfsr B = {0, 0b11110001110000001111000001000101, 32};
	struct lfsr C = {0, 0b101010000111001101111101000100100, 33};
	word S = 0;
	     
	// Initialization of S
	for (int i = 31;i >= 0;i--) {
		S = clock_S(S, testBit(iv, i));
	}
	
	S = clock_S(S, dir);
	
	for (int i = 63;i >= 0;i--) {
		S = clock_S(S, testBit(key, i));
	}
	
	for (int i = 0;i < 128;i++) {
		S = clock_S(S, 0);
	}

	// Initialization of LFSRs
	for (int i = 0;i < 64;i++) {
		A.state = clock_lfsr(A, testBit(S, i));
	}

	for (int i = 0;i < 64;i++) {
		B.state = clock_lfsr(B, testBit(S, (i + 16) % 64));
	}
	
	for (int i = 0;i< 64;i++) {
		C.state = clock_lfsr(C, testBit(S, (i + 32) % 64));
	}

	if (A.state == 0) A.state = 1;
	if (B.state == 0) B.state = 1;
	if (C.state == 0) C.state = 1;

	// Encrypting the message
	char* ciphertext = malloc(1601*sizeof(char));
	unsigned char keystream;
	for (int i = 0;i < 1601;i++) {
		if (i == ptSize) {
			break;
		}
		keystream = 0;
		for (int j = 0;j < 8;j++) {
			bit f_A = f(testBit(A.state, 22), testBit(A.state, 0), testBit(A.state, 13), testBit(A.state, 21), testBit(A.state, 25), testBit(A.state, 2), testBit(A.state, 7));
			bit f_B = f(testBit(B.state, 12), testBit(B.state, 27), testBit(B.state, 0), testBit(B.state, 1), testBit(B.state, 29), testBit(B.state, 21), testBit(B.state, 5));
			bit f_C = f(testBit(C.state, 10), testBit(C.state, 30), testBit(C.state, 32), testBit(C.state, 3), testBit(C.state, 19), testBit(C.state, 0), testBit(C.state, 4));

			keystream >>= 1;
			keystream |= (f_A ^ f_B ^ f_C) << 7;

			A.state = clock_lfsr(A, 0);
			B.state = clock_lfsr(B, 0);
			C.state = clock_lfsr(C, 0);
		}
		ciphertext[i] = plaintext[i] ^ keystream;
	}

	char* outFileName = malloc(133*sizeof(char));
	sprintf(outFileName, "%.128s-enc", inFileName);
	FILE* outFile = fopenErr(outFileName, "w");
	writeBytes(outFile, ciphertext, ptSize);
	fclose(outFile);

	printf("Wrote encrypted file to %s\n", outFileName);
}
