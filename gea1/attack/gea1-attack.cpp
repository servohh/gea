#include <NTL/GF2.h>
#include <NTL/vec_GF2.h>
#include <NTL/mat_GF2.h>
#include <NTL/BasicThreadPool.h>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "define-matrices.h"

#define CONST_L 72
#define ENTRY_SIZE (CONST_L + 24) / 8
#define TAPS_A 0b1011000111011001000110111011101
#define TAPS_C 0b001001000101111101100111000010101
#define SHIFT_A 0
#define SHIFT_C 32

#define SIZE_V 1
#define SIZE_TAC 1l << 24
#define SIZE_UB 1l << 32

#define NUM_THREADS 16
#define NUM_KEYS 1024 // Create space for 1024 candidate keys by default. We expect 1 (the correct key) so this is more than sufficient
#define KEYS_FILENAME "candidate_keys"

using namespace NTL;
using namespace std;
mutex keysMutex;

// Generate a vector from the space spanned by A, using l bits of n as scalars
void span(Vec<GF2>& v, Mat<GF2>& A, long n, int l)
{
	Vec<GF2> multVec;
	for (int i = (l - 1);i >= 0;i--) {
		multVec.append(conv<GF2>((n >> i) % 2));
	}
	v = transpose(A) * multVec;
	multVec.kill();
}

// GEA-1 nonlinear function
GF2 f(GF2 x0, GF2 x1, GF2 x2, GF2 x3, GF2 x4, GF2 x5, GF2 x6)
{
        return x0+x2+x5+x6 * x0+x3+x5+x6 * x0+x1+x5+x6 * x1+x2+x5+x6 * x0+x2+x3+x6 * x1+x3+x4+x6 * x1+x3+x5+x6 * x0+x2+x4 * x0+x2+x3 * x0+x1+x3 * x0+x2+x6 * x0+x1+x4 * x0+x1+x6 * x1+x2+x6 * x2+x5+x6 * x0+x3+x5 * x1+x4+x6 * x1+x2+x5 * x0+x3 * x0+x5 * x1+x3 * x1+x5 * x1+x6 * x0+x2 * x1 * x2+x3 * x2+x5 * x2+x6 * x4+x5 * x5+x6 * x2 * x3 * x5;
}

char f(char x0, char x1, char x2, char x3, char x4, char x5, char x6)
{
        return x0&x2&x5&x6 ^ x0&x3&x5&x6 ^ x0&x1&x5&x6 ^ x1&x2&x5&x6 ^ x0&x2&x3&x6 ^ x1&x3&x4&x6 ^ x1&x3&x5&x6 ^ x0&x2&x4 ^ x0&x2&x3 ^ x0&x1&x3 ^ x0&x2&x6 ^ x0&x1&x4 ^ x0&x1&x6 ^ x1&x2&x6 ^ x2&x5&x6 ^ x0&x3&x5 ^ x1&x4&x6 ^ x1&x2&x5 ^ x0&x3 ^ x0&x5 ^ x1&x3 ^ x1&x5 ^ x1&x6 ^ x0&x2 ^ x1 ^ x2&x3 ^ x2&x5 ^ x2&x6 ^ x4&x5 ^ x5&x6 ^ x2 ^ x3 ^ x5;
}

// Search table tab for output string n. Returns the corresponding entry if found, else returns null pointer
unsigned char* search(unsigned char* tab, long size, unsigned char* n)
{
	long left = 0;
	long right = 0;
	while (left <= right) {
		long pos = (left + right) >> 1;
		int cmp = memcmp(tab + pos + 3, n, (CONST_L/8)*sizeof(unsigned char));
		if (cmp < 0) {
			left = pos + 1;
		} else if (cmp > 0) {
			right = pos + 1;
		} else {
			return tab + pos;
		}
	}
	return 0;
}

// Clock function for GEA-1 register S. Used to recover session key
unsigned long S_clock(unsigned long s, char input)
{
	char output = s & 1;
	char fOut = f((char)((s >> 3) & 1), (char)((s >> 12) & 1), (char)((s >> 22) & 1), (char)((s >> 38) & 1), (char)((s >> 42) & 1), (char)((s >> 55) & 1), (char)((s >> 63) & 1));
	s >>= 1;
	s ^= ((unsigned long)(input ^ fOut ^ output)) << 63;
	return s;
}

// Reverse clock function for GEA-1 register S with input bit 0. Used to recover session key
unsigned long S_clockBackwards(unsigned long s)
{
	char inBit = (char)((s >> 63) & 1);
	s <<= 1;
	char fOut = f((char)((s >> 3) & 1), (char)((s >> 12) & 1), (char)((s >> 22) & 1), (char)((s >> 38) & 1), (char)((s >> 42) & 1), (char)((s >> 55) & 1), (char)((s >> 63) & 1));
	s ^= inBit ^ fOut;
	return s;
}

// Recover session key from the vector form of s
unsigned long recoverSessionKey(Vec<GF2> vec_s, unsigned int iv, char dir)
{
	unsigned long a = 0;
	for (int i = 0;i < 64;i++) {
		a <<= 1;
		a += IsOne(vec_s[i]);
	}
	for (int i = 0;i < 128;i++) {
		a = S_clockBackwards(a);
	}
	unsigned long b = 0;
	for (int i = 0;i < 32;i++) {
		b = S_clock(b, (char) (iv >> (31 - i)));
	}
	b = S_clock(b, dir);
	for (int i = 0;i < 64;i++) {
		b = S_clock(b, 0);
	}
	return a ^ b;
}

// Save candidate key to file - thread-safe version. Return new keysIndex
void saveCandidateKey(unsigned long key)
{
	const lock_guard<mutex> lock(keysMutex);
	ofstream candidateKeyFile(KEYS_FILENAME, ios::out | ios::binary | ios::app);
	for (int i = 7;i >= 0;i--) {
		candidateKeyFile << (unsigned char) (key >> (8 * i));
	}
	candidateKeyFile.close();
}

int main(int argc, char* argv[])
{
	SetNumThreads(NUM_THREADS);

	if (argc != 4) {
		cout << "Usage: gea1-attack {keystream-file} {iv-file} {dir}" << "\n";
		return 0;
	}

	unsigned char* keystream = (unsigned char*) malloc(CONST_L/8 * sizeof(unsigned char));
	unsigned int iv = 0;
	ifstream keystreamFile(argv[1], ios::in | ios::binary);
	for (int i = 0;i < CONST_L/8;i++) {
		keystream[i] = keystreamFile.get();
	}
	keystreamFile.close();
	ifstream ivFile(argv[2], ios::in | ios::binary);
	for (int i = 0;i < 4;i++) {
		iv <<= 8;
		iv ^= ((unsigned int)ivFile.get()) & 0xFF;
	}
	ivFile.close();	
	char dir = (char) atoi(argv[3]);

	cout << "Successfully read keystream and IV" << "\n";

	Mat<GF2> U_B;
	Mat<GF2> T_AC;
	Mat<GF2> V;
	defineMatrices(U_B, T_AC, V);

	Mat<GF2> A_Mat;
	A_Mat.SetDims(31, 31);
	companionMatrix(A_Mat, TAPS_A);
	Mat<GF2> M_A;
	M_A.SetDims(64, 31);
	getInitMatrix(M_A, A_Mat, SHIFT_A);
	transpose(M_A, M_A);

	Mat<GF2> C_Mat;
	C_Mat.SetDims(33, 33);
	companionMatrix(C_Mat, TAPS_C);
	Mat<GF2> M_C;
	M_C.SetDims(64, 33);
	getInitMatrix(M_C, C_Mat, SHIFT_C);
	transpose(M_C, M_C);

	NTL_EXEC_RANGE(SIZE_V, first, last)
	for (long i = first;i < last;i++) {
		Vec<GF2> vec_v;
		Vec<GF2> vec_u;
		Vec<GF2> vec_t;
		Vec<GF2> vec_s;
		vec_s.SetLength(64);
		Vec<GF2> A_uv;
		Vec<GF2> C_uv;	
		unsigned char* tab_v = (unsigned char*) malloc((ENTRY_SIZE * SIZE_TAC) * sizeof(unsigned char));
		unsigned char* array_A = (unsigned char*) malloc(CONST_L/8 * sizeof(unsigned char));
		unsigned char* array_B = (unsigned char*) malloc(CONST_L/8 * sizeof(unsigned char));
		unsigned char* array_C = (unsigned char*) malloc(CONST_L/8 * sizeof(unsigned char));
		span(vec_v, V, i, 8);
		char* string_i = (char*) malloc(3 * sizeof(char));
		sprintf(string_i, "%li", i);
		ifstream table(string_i, ios::in | ios::binary);
		for (int a = 0;a < SIZE_TAC;a++) {
			tab_v[a] = (unsigned char) table.get();
		}
		table.close();
		free(string_i);
		for (long j = 0;j < SIZE_UB;j++) {
			span(vec_u, U_B, j, 32);
			A_uv = M_A * (vec_u + vec_v);
			C_uv = M_C * (vec_u + vec_v);
			GF2 outBitA;
			GF2 outBitC;
			int outA = 0;
			int outC = 0;
			int outLength = 0;
			int index_ac = 0;	
			for (int k = 0;k < CONST_L;k++) {
				outBitA = f(A_uv[22], A_uv[0], A_uv[13], A_uv[21], A_uv[25], A_uv[2], A_uv[7]);
				outBitC = f(C_uv[10], C_uv[30], C_uv[32], C_uv[3], C_uv[19], C_uv[0], C_uv[4]);
				A_uv = A_Mat * A_uv;
				C_uv = C_Mat * C_uv;
				outA <<= 1;
				outC <<= 1;
				outA += IsOne(outBitA);
				outC += IsOne(outBitC);
				outLength++;
				if (outLength == 8) {
					array_A[index_ac] = outA;
					array_C[index_ac] = outC;
					index_ac++;
					outA = 0;
					outC = 0;
					outLength = 0;
				}
			}
			if (outA != 0) array_A[index_ac] = outA;
			if (outC != 0) array_C[index_ac] = outC;
			for (int k = 0;k < CONST_L/8;k++) {
				array_B[k] = array_A[k] ^ array_C[k] ^ keystream[k];
			}
			unsigned char* B_entry = search(tab_v, (ENTRY_SIZE * SIZE_TAC), array_B);
			if (B_entry != 0) {
				cout << "Found 1 candidate key." << "\n";
				long t = 0;
				for (int a = 0;a < 3;a++) {
					for (int b = 0;b < 8;b++) {
						t <<= 1;
                                		t += B_entry[a] >> ((2 - b) * 8);
                        		}
				}
				span(vec_t, T_AC, t, 24);
				vec_s = vec_u + vec_t + vec_v;
				unsigned long key = recoverSessionKey(vec_s, iv, dir);
				saveCandidateKey(key);
			}
		} 
		vec_v.kill();
		vec_u.kill();
		vec_t.kill();
		A_uv.kill();
		C_uv.kill();
		free(tab_v);
		free(array_A);
		free(array_B);
		free(array_C);
	}
	NTL_EXEC_RANGE_END
	cout << "Candidate keys written to file " << KEYS_FILENAME << "\n";
}
