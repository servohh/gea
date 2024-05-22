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

// GEA-1 nonlinear function.
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

// Clock function for the non-linear feedback register S
word clock_s(word reg, bit in) 
{
	bit output = reg & 1;
	bit fOut = f(getBitAtPos(reg, 3), getBitAtPos(reg, 12), getBitAtPos(reg, 22), getBitAtPos(reg, 38), getBitAtPos(reg, 42), getBitAtPos(reg, 55), getBitAtPos(reg, 63));
	reg >>= 1;
	reg ^= ((word)(in ^ fOut ^ output)) << 63;
	return reg;
}

// fopen with error checking
FILE* fopen_err(char* filename, char* mode)
{
	FILE* file = fopen(filename, mode);
	if (file == NULL) {
		printf("gea1-enc: error opening file %s\n", filename);
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
		printf("gea1-enc\nUsage: %s FILE [IV] [DIR] KEY\nTakes 64-bit key from file KEY and encrypts FILE using GEA-1 algorithm. If IV and DIR are not given, they will be randomly chosen.\n", argv[0]);
		return 0;
	}

	bool chosenDir = false;
	bool chosenIV = false;
	char* inFileName = argv[1];
	char* keyFileName;
	char* ivFileName;
	bit dir;

	int i = 1;
	while (i < argc) {
		if (isdigit((int)*argv[i])) {
			chosenDir = true;
			dir = atoi(argv[i]);
			break;
		}
		i++;
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
		ivFileName = argv[((i + 1) % 2) + 2]; // if the argument number of dir is 2 this will be 3, and vice versa
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

	char* plaintext = malloc(1501*sizeof(char));
	int ptSize;
	FILE* inFile = fopen_err(inFileName, "r");
	if (inFile == NULL) return 1;
	ptSize = read_bytes(inFile, plaintext, 1501);
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

	struct lfsr A = {0, 0b1011101110110001001101110001101, 31};
	struct lfsr B = {0, 0b11110001110000001111000001000101, 32};
	struct lfsr C = {0, 0b101010000111001101111101000100100, 33};
	word regS = 0;
	     
	// Initialization of S
	for (int i = 0;i < 32;i++) {
		regS = clock_s(regS, getBitAtPos(iv, i));
	}
	
	regS = clock_s(regS, dir);
	
	for (int i = 0;i < 64;i++) {
		regS = clock_s(regS, getBitAtPos(key, i));
	}
	
	for (int i = 0;i < 128;i++) {
		regS = clock_s(regS, 0);
	}

	printf("%lx\n", regS);
	
	// Initialization of LFSRs
	for (int i = 0;i < 64;i++) {
		A.state = clock_lfsr(A, getBitAtPos(regS, i));
		B.state = clock_lfsr(B, getBitAtPos(regS, (i + 16) % 64));
		C.state = clock_lfsr(C, getBitAtPos(regS, (i + 32) % 64));
	}

	if (A.state == 0) A.state = 1;
	if (B.state == 0) B.state = 1;
	if (C.state == 0) C.state = 1;

	// Encrypting the message
	char* ciphertext = malloc(1601*sizeof(char));
	unsigned char keystream;
	for (int i = 0;i < 1501;i++) {
		if (i == ptSize) {
			break;
		}
		keystream = 0;
		for (int j = 0;j < 8;j++) {
			bit f_A = f(getBitAtPos(A.state, 22), getBitAtPos(A.state, 0), getBitAtPos(A.state, 13), getBitAtPos(A.state, 21), getBitAtPos(A.state, 25), getBitAtPos(A.state, 2), getBitAtPos(A.state, 7));
			bit f_B = f(getBitAtPos(B.state, 12), getBitAtPos(B.state, 27), getBitAtPos(B.state, 0), getBitAtPos(B.state, 1), getBitAtPos(B.state, 29), getBitAtPos(B.state, 21), getBitAtPos(B.state, 5));
			bit f_C = f(getBitAtPos(C.state, 10), getBitAtPos(C.state, 30), getBitAtPos(C.state, 32), getBitAtPos(C.state, 3), getBitAtPos(C.state, 19), getBitAtPos(C.state, 0), getBitAtPos(C.state, 4));

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
	FILE* outFile = fopen_err(outFileName, "w");
	write_bytes(outFile, ciphertext, ptSize);
	fclose(outFile);

	printf("Wrote encrypted file to %s\n", outFileName);
}
