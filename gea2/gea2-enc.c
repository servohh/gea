#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>

typedef uint64_t word;
typedef uint8_t bit;

// Returns 0 or 1 depending on the value of the bit at position pos in n.
// e.g.: getBitAtPos(0b1000, 3) returns 1, getBitAtPos(0b1000, 2) returns 0.
// Assumes n <= 63
bit getBitAtPos(word n, unsigned int pos)
{
	return (bit)((n >> pos) & 1);
}

// GEA-2 nonlinear function.
// Assumes all inputs are in {0, 1}
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

// Struct to represent a 97-bit unsigned int
struct uint97
{
	word upper; // Upper 33 bits
	word lower; // Lower 64 bits
};

bit uint97_getBit(struct uint97 n, unsigned int pos)
{
	if (pos < 64) return (bit)((n.lower >> pos) & 1);
	else return (bit)((n.upper >> pos) & 1);
}

// Galois LFSR clock function
word clock_lfsr(struct lfsr reg, bit in) 
{
	bit output = reg.state & 1;
	output ^= in;
	reg.state >>= 1;
	if (output) {
		reg.state ^= reg.taps;
	}
	return reg.state;
}

// Clock function for the non-linear feedback register W
struct uint97 clock_w(struct uint97 reg, bit in) 
{
	bit output = reg.lower & 1;
	bit carry = reg.upper & 1;
	bit fOut = f(getBitAtPos(reg.lower, 4), getBitAtPos(reg.lower, 18), getBitAtPos(reg.lower, 33), getBitAtPos(reg.lower, 57), getBitAtPos(reg.lower, 63), getBitAtPos(reg.upper, 19), getBitAtPos(reg.upper, 32));
	reg.upper >>= 1;
	reg.lower >>= 1;
	reg.lower ^= (word)carry << 63; // Carry bit from upper to lower
	reg.upper ^= ((word)(in ^ fOut ^ output)) << 32;
	return reg;
}

// fopen with error checking
FILE* fopen_err(char* filename, char* mode)
{
	FILE* file = fopen(filename, mode);
	if (file == NULL) {
		printf("gea2-enc: error opening file %s\n", filename);
	}
	return file;
}

// Read n bytes from file into buf, or until EOF is reached.
// Returns the number of bytes read.
int read_bytes(FILE* file, char* buf, int n)
{
	char c;
	int i = 0;
	while (i < n) {
		c = fgetc(file);
		if (feof(file)) break;
		buf[i] = c;
		i++;
	}
	return i;
}

// Write n bytes from buf into file.
void write_bytes(FILE* file, char* buf, int n)
{
	char c;
	for (int i = 0;i < n;i++) {
		c = buf[i];
                fputc(c, file);
        }
}

int main(int argc, char* argv[])
{
	srand(time(NULL));

	// Managing arguments. FILE and KEY are required - IV and DIR are optional
	if (argc < 3 || argc > 5) {
		printf("gea2-enc\nUsage: %s FILE [IV] [DIR] KEY\nTakes 64-bit key from file KEY and encrypts FILE using GEA-2 algorithm. If IV and DIR are not given, they will be randomly chosen.\n", argv[0]);
		return 0;
	}

	bool chosenDir = false;
	bool chosenIV = false;
	char* inFileName = argv[1];
	char* keyFileName;
	char* ivFileName;
	bit dir;

	int dirI = 1; // Will hold the position of the dir bit in argv
	while (dirI < argc) {
		if (isdigit((int)*argv[dirI])) {
			chosenDir = true;
			dir = atoi(argv[dirI]);
			break;
		}
		dirI++;
	}
	
	if (argc == 3) {
		keyFileName = argv[2];
	} else if (argc == 4) {
		keyFileName = argv[3];
		if (!chosenDir) {
			chosenIV = true;
			ivFileName = argv[2];
		}
	} else if (argc == 5 && chosenDir) {
		chosenIV = true;
		ivFileName = argv[((dirI + 1) % 2) + 2]; // if the argument number of dir is 2 this will be 3, and vice versa
		keyFileName = argv[4];
	}

	// Initializing variables
	word key = 0;
	char* keyBuf = malloc(8*sizeof(char));
	FILE* keyFile = fopen_err(keyFileName, "r");
	if (keyFile == NULL) return 1;
	read_bytes(keyFile, keyBuf, 8);
	fclose(keyFile);
	for (int i = 0;i < 8;i++) {
		key <<= 8;
		key ^= ((word)keyBuf[i]) & 0xFF;
	}
	free(keyBuf);

	char* plaintext = malloc(1600*sizeof(char));
	int ptSize;
	FILE* inFile = fopen_err(inFileName, "r");
	if (inFile == NULL) return 1;
	ptSize = read_bytes(inFile, plaintext, 1600);
	fclose(inFile);

	if (chosenDir == false) dir = rand() % 2;
	printf("dir is %u\n", dir);

	uint32_t iv = 0;

	if (chosenIV) {
		char* ivBuf = malloc(4*sizeof(char));
		FILE* ivFile = fopen(ivFileName, "r");
		if (ivFile == NULL) return 1;
		read_bytes(ivFile, ivBuf, 4);
		fclose(ivFile);
		for (int i = 0;i < 4;i++) {
			iv <<= 8;
			iv ^= ((uint32_t)ivBuf[i]) & 0xFF;
		}
		printf("Successfully read IV from %s\n", ivFileName);
	} else {
		iv = rand();
		char* ivOutBuf = malloc(4*sizeof(char));
		char* ivOutFileName = malloc(132*sizeof(char));
		sprintf(ivOutFileName, "%.128s-iv", inFileName);
		FILE* ivOutFile = fopen_err(ivOutFileName, "w");
		if (ivOutFile == NULL) return 1;
		for (int i = 0;i < 4;i++) {
			ivOutBuf[i] = (iv >> (24 - (i*8))) & 0xFF;
		}
		write_bytes(ivOutFile, ivOutBuf, 4);
		fclose(ivOutFile);
		free(ivOutBuf);
		printf("Wrote generated IV to %s\n", ivOutFileName);
	}

	struct lfsr D = {0, 0b11010010110011010101111111001, 29};
	struct lfsr A = {0, 0b1011101110110001001101110001101, 31};
	struct lfsr B = {0, 0b11110001110000001111000001000101, 32};
	struct lfsr C = {0, 0b101010000111001101111101000100100, 33};
	struct uint97 regW = {0, 0};
	     
	// Initialization of W
	for (int i = 0;i < 32;i++) {
		regW = clock_w(regW, getBitAtPos(iv, i));
	}
	
	regW = clock_w(regW, dir);
	
	for (int i = 0;i < 64;i++) {
		regW = clock_w(regW, getBitAtPos(key, i));
	}
	
	for (int i = 0;i < 194;i++) {
		regW = clock_w(regW, 0);
	}

	// Initialization of LFSRs
	for (int i = 0;i < 97;i++) {
		D.state = clock_lfsr(D, uint97_getBit(regW, i));
		A.state = clock_lfsr(A, uint97_getBit(regW, (i + 16) % 97));
		B.state = clock_lfsr(B, uint97_getBit(regW, (i + 33) % 97));
		C.state = clock_lfsr(C, uint97_getBit(regW, (i + 51) % 97));
	}

	if (D.state == 0) D.state = 1;
	if (A.state == 0) A.state = 1;
	if (B.state == 0) B.state = 1;
	if (C.state == 0) C.state = 1;

	// Encrypting the message
	char* ciphertext = malloc(1600*sizeof(char));
	unsigned char keystream;
	for (int i = 0;i < 1600;i++) {
		if (i == ptSize) {
			break;
		}
		keystream = 0;
		for (int j = 0;j < 8;j++) {
			bit f_D = f(getBitAtPos(D.state, 12), getBitAtPos(D.state, 23), getBitAtPos(D.state, 3), getBitAtPos(D.state, 0), getBitAtPos(D.state, 10), getBitAtPos(D.state, 27), getBitAtPos(D.state, 17));
			bit f_A = f(getBitAtPos(A.state, 22), getBitAtPos(A.state, 0), getBitAtPos(A.state, 13), getBitAtPos(A.state, 21), getBitAtPos(A.state, 25), getBitAtPos(A.state, 2), getBitAtPos(A.state, 7));
			bit f_B = f(getBitAtPos(B.state, 12), getBitAtPos(B.state, 27), getBitAtPos(B.state, 0), getBitAtPos(B.state, 1), getBitAtPos(B.state, 29), getBitAtPos(B.state, 21), getBitAtPos(B.state, 5));
			bit f_C = f(getBitAtPos(C.state, 10), getBitAtPos(C.state, 30), getBitAtPos(C.state, 32), getBitAtPos(C.state, 3), getBitAtPos(C.state, 19), getBitAtPos(C.state, 0), getBitAtPos(C.state, 4));

			keystream >>= 1;
			keystream |= (f_D ^ f_A ^ f_B ^ f_C) << 7;

			D.state = clock_lfsr(D, 0);
			A.state = clock_lfsr(A, 0);
			B.state = clock_lfsr(B, 0);
			C.state = clock_lfsr(C, 0);
		}
		ciphertext[i] = plaintext[i] ^ keystream;
	}

	char* outFileName = malloc(133*sizeof(char));
	sprintf(outFileName, "%.128s-enc", inFileName);
	FILE* outFile = fopen_err(outFileName, "w");
	write_bytes(outFile, ciphertext, ptSize);
	fclose(outFile);

	printf("Wrote encrypted file to %s\n", outFileName);
}
