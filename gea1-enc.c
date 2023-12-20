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
	if (n & bitMask(pos) > 0) {
		return 1;
	} else {
		return 0;
	}
}

// GEA-1 nonlinear function
bit f(bit x0, bit x1, bit x2, bit x3, bit x4, bit x5, bit x6) 
{
	return (x0*x2*x5*x6 + x0*x3*x5*x6 + x0*x1*x5*x6 + x1*x2*x5*x6 + x0*x2*x3*x6 + x1*x3*x4*x6 + x1*x3*x5*x6 + x0*x2*x4 + x0*x2*x3 + x0*x1*x3 + x0*x2*x6 + x0*x1*x4 + x0*x1*x6 + x1*x2*x6 + x2*x5*x6 + x0*x3*x5 + x1*x4*x6 + x1*x2*x5 + x0*x3 + x0*x5 + x1*x3 + x1*x5 + x1*x6 + x0*x2 + x1 + x2*x3 + x2*x5 + x2*x6 + x4*x5 + x5*x6 + x2 + x3 + x5) % 2;
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
	bit output = testBit(reg.state, reg.length - 1);
	output ^= in;
	reg.state <<= 1;
	if (output == 1) {
		reg.state ^= reg.taps;
	}
	return reg.state;
}

// Clock function for the non-linear feedback register S
word clock_S(word reg, bit in) 
{
	bit output = testBit(reg, 63);
	bit fOut = f(testBit(reg, 60), testBit(reg, 51), testBit(reg, 41), testBit(reg, 25), testBit(reg, 21), testBit(reg, 8), testBit(reg, 0));
	reg <<= 1;
	reg ^= in ^ fOut ^ output;
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
	FILE* keyFile = fopenErr(keyFileName, "r");
	if (keyFile == NULL) return 1;
	fscanf(keyFile, "%x", &key);
	fclose(keyFile);

	char* plaintext = malloc(1601*sizeof(char));
	FILE* inFile = fopenErr(inFileName, "r");
	if (inFile == NULL) return 1;
	char inChar;
	for (int i = 0;i < 1601;i++) {
		inChar = fgetc(inFile);
		plaintext[i] = inChar;
		if (inChar == EOF) break;
	}
	fclose(inFile);

	if (chosenDir == false) dir = rand() % 2;
	printf("dir is %u\n", dir);

	uint32_t iv = 0;

	if (chosenIV == 0) {
		iv = rand();
		char* ivOutFileName = malloc(132*sizeof(char));
		sprintf(ivOutFileName, "%.128s-iv", inFileName);
		FILE* ivOutFile = fopenErr(ivOutFileName, "w");
		if (ivOutFile == NULL) return 1;
		fprintf(ivOutFile, "%x", iv);
		fclose(ivOutFile);
		printf("Wrote generated IV to %s\n", ivOutFileName);
	} else {
		FILE* ivFile = fopen(ivFileName, "r");
		if (ivFile == NULL) return 1;
		fscanf(ivFile, "%x", &iv);
		fclose(ivFile);
		printf("Successfully read IV from %s\n", ivFileName);
	}

	struct lfsr A = {0, 0b1101100011101100100011011101110, 31};
	struct lfsr B = {0, 0b11010001000001111000000111000111, 32};
	struct lfsr C = {0, 0b100100100010111110110011100001010, 33};
	word S = 0;
	
	// Initialization of S
	for (int i = 0;i < 8*sizeof(iv);i++) {
		S = clock_S(S, testBit(iv, i));
	}
	
	S = clock_S(S, dir);
	
	for (int i = 0;i < 8*sizeof(key);i++) {
		S = clock_S(S, testBit(key, i));
	}
	
	for (int i = 0;i < 128;i++) {
		S = clock_S(S, 0);
	}

	// Initialization of LFSRs
	for (int i = 0;i < 8*sizeof(S);i++) {
		A.state = clock_lfsr(A, testBit(S, i));
	}

	for (int i = 0;i < 8*sizeof(S);i++) {
		B.state = clock_lfsr(B, testBit(S, (i + 16) % 8*sizeof(S)));
	}
	
	for (int i = 0;i< 8*sizeof(S);i++) {
		C.state = clock_lfsr(C, testBit(S, (i + 32) % 8*sizeof(S)));
	}

	if (A.state == 0) A.state = 1;
	if (B.state == 0) B.state = 1;
	if (C.state == 0) C.state = 1;

	// Encrypting the message
	char* ciphertext = malloc(1601*sizeof(char));
	unsigned char keystream;
	for (int i = 0;i < 1601;i++) {
		if (plaintext[i] == EOF) break;
		keystream = 0;
		for (int i = 0;i < 8;i++) {
			bit f_A = f(testBit(A.state, 8), testBit(A.state, 30), testBit(A.state, 17), testBit(A.state, 9), testBit(A.state, 5), testBit(A.state, 23), testBit(A.state, 28));
			bit f_B = f(testBit(B.state, 18), testBit(B.state, 4), testBit(B.state, 31), testBit(B.state, 30), testBit(B.state, 2), testBit(B.state, 10), testBit(B.state, 26));
			bit f_C = f(testBit(C.state, 22), testBit(C.state, 2), testBit(C.state, 0), testBit(C.state, 29), testBit(C.state, 13), testBit(C.state, 32), testBit(C.state, 28));

			keystream |= f_A ^ f_B ^ f_C;
			keystream <<= 1;

			A.state = clock_lfsr(A, 0);
			B.state = clock_lfsr(B, 0);
			C.state = clock_lfsr(C, 0);
		}
		ciphertext[i] = plaintext[i] ^ keystream;
	}

	char* outFileName = malloc(133*sizeof(char));
	sprintf(outFileName, "%.128s-enc", inFileName);
	FILE* outFile = fopenErr(outFileName, "w");
	int out = fputs(ciphertext, outFile);
	if (out == EOF) return 1;
	fclose(outFile);

	printf("Wrote encrypted file to %s\n", outFileName);
}
