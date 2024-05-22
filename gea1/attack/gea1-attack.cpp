#include <NTL/GF2.h>
#include <NTL/vec_GF2.h>
#include <NTL/mat_GF2.h>
#include <NTL/BasicThreadPool.h>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "attack-parms.h"
#include "define-matrices.h"
#include "gea1-func.h"

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


// GEA-1 non-linear function
char f(char x0, char x1, char x2, char x3, char x4, char x5, char x6)
{
        return x0&x2&x5&x6 ^ x0&x3&x5&x6 ^ x0&x1&x5&x6 ^ x1&x2&x5&x6 ^ x0&x2&x3&x6 ^ x1&x3&x4&x6 ^ x1&x3&x5&x6 ^ x0&x2&x4 ^ x0&x2&x3 ^ x0&x1&x3 ^ x0&x2&x6 ^ x0&x1&x4 ^ x0&x1&x6 ^ x1&x2&x6 ^ x2&x5&x6 ^ x0&x3&x5 ^ x1&x4&x6 ^ x1&x2&x5 ^ x0&x3 ^ x0&x5 ^ x1&x3 ^ x1&x5 ^ x1&x6 ^ x0&x2 ^ x1 ^ x2&x3 ^ x2&x5 ^ x2&x6 ^ x4&x5 ^ x5&x6 ^ x2 ^ x3 ^ x5;
}

// Search table tab for output string n. Returns the corresponding entry if found, else returns null pointer
unsigned char* search(unsigned char* tab, long size, unsigned char* n)
{
	long left = 0;
	long right = size - 1;
	while (left <= right) {
		long pos = ((left + right) >> 1);
		int cmp = memcmp(tab + (pos * ENTRY_SIZE) + 3, n, (CONST_L/8)*sizeof(unsigned char));
		if (cmp < 0) {
			left = pos + 1;
		} else if (cmp > 0) {
			right = pos - 1;
		} else {
			return tab + (pos * ENTRY_SIZE);
		}
	}
	return 0;
}

struct lfsr getLFSR(Vec<GF2> vect, uint64_t taps)
{
	uint64_t state = 0;
	int size = vect.length();
	for (int i = 0;i < size;i++) {
		state >>= 1;
		state |= IsOne(vect[i]) << (size - 1);
	}
	struct lfsr reg = {state, taps, (unsigned int) size};
	return reg;
}

char bit_at_pos(unsigned long n, unsigned int pos)
{
        return (char)((n >> pos) & 1);
}

// Clock function for GEA-1 register S. Used to recover session key
unsigned long S_clock(unsigned long s, char input)
{
	char output = s & 1;
	s >>= 1;
        char fOut = f(bit_at_pos(s, 3), bit_at_pos(s, 12), bit_at_pos(s, 22), bit_at_pos(s, 38), bit_at_pos(s, 42), bit_at_pos(s, 55), bit_at_pos(s, 63));
	s ^= ((unsigned long)(input ^ fOut ^ output)) << 63;
	return s;
}

// Reverse clock function for GEA-1 register S with input bit 0. Used to recover session key
unsigned long S_clockBackwards(unsigned long s)
{
	char inBit = (char)((s >> 63) & 1);
	s <<= 1;
        char fOut = f(bit_at_pos(s, 3), bit_at_pos(s, 12), bit_at_pos(s, 22), bit_at_pos(s, 38), bit_at_pos(s, 42), bit_at_pos(s, 55), bit_at_pos(s, 63));
	s ^= inBit ^ fOut;
	return s;
}

// Recover session key from the vector form of s
unsigned long recoverSessionKey(Vec<GF2> vec_s, unsigned int iv, char dir)
{
        unsigned long key = 0;
        unsigned long a = 0;
	for (int i = 0;i < 64;i++) {
		a >>= 1;
		a |= (IsOne(vec_s[i])) << 63;
	}
        for (int i = 0;i < 128;i++) {
                a = S_clockBackwards(a);
        }
        unsigned long b = 0;
        for (int i = 0;i < 32;i++) {
                b = S_clock(b, bit_at_pos(iv, i));
        }
        b = S_clock(b, dir);
        for (int i = 0;i < 64;i++) {
                char a_i = (a >> i) & 1;
                char output = b & 1;
                char fOut = f(bit_at_pos(b, 3), bit_at_pos(b, 12), bit_at_pos(b, 22), bit_at_pos(b, 38), bit_at_pos(b, 42), bit_at_pos(b, 55), bit_at_pos(b, 63));
                key >>= 1;
                key ^= ((unsigned long)(a_i ^ fOut ^ output)) << 63;
                b >>= 1;
                b ^= ((unsigned long)(a_i)) << 63;
        }
        return key;
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
	A_Mat.kill();

	Mat<GF2> C_Mat;
	C_Mat.SetDims(33, 33);
	companionMatrix(C_Mat, TAPS_C);
	Mat<GF2> M_C;
	M_C.SetDims(64, 33);
	getInitMatrix(M_C, C_Mat, SHIFT_C);
	transpose(M_C, M_C);
	C_Mat.kill();

	NTL_EXEC_RANGE(SIZE_V, first, last)
	for (long i = first;i < last;i++) {
		Vec<GF2> vec_v;
		Vec<GF2> vec_u;
		Vec<GF2> vec_uv;
		Vec<GF2> vec_t;
		Vec<GF2> A_uv;
		Vec<GF2> C_uv;	
		unsigned char* tab_v = (unsigned char*) malloc((ENTRY_SIZE * SIZE_TAC) * sizeof(unsigned char));
		unsigned char* array_B = (unsigned char*) malloc(CONST_L/8 * sizeof(unsigned char));
		span(vec_v, V, i, 8);
		char* string_i = (char*) malloc(3 * sizeof(char));
		sprintf(string_i, "%li", i);
		ifstream table(string_i, ios::in | ios::binary);
		for (int a = 0;a < (ENTRY_SIZE * SIZE_TAC);a++) {
			tab_v[a] = (unsigned char) table.get();
		}
		table.close();
		free(string_i);
		for (long j = 0;j < SIZE_UB;j++) {
			span(vec_u, U_B, j, 32);
			vec_uv = vec_u + vec_v;
			A_uv = M_A * vec_uv;
			C_uv = M_C * vec_uv;
			struct lfsr A = getLFSR(A_uv, 0b1011101110110001001101110001101);
			struct lfsr C = getLFSR(C_uv, 0b101010000111001101111101000100100);
			if (A.state == 0) A.state = 1;
			if (C.state == 0) C.state = 1;
			deriveOutputB(A, C, keystream, array_B);
			unsigned char* B_entry = search(tab_v, (SIZE_TAC), array_B);
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
				Vec<GF2> vec_s;
				vec_s = vec_u + vec_t + vec_v;
				unsigned long key = recoverSessionKey(vec_s, iv, dir);
				saveCandidateKey(key);
				vec_s.kill();
			}
		} 
		vec_v.kill();
		vec_u.kill();
		vec_uv.kill();
		vec_t.kill();
		A_uv.kill();
		C_uv.kill();
		free(tab_v);
		free(array_B);
	}
	NTL_EXEC_RANGE_END
	cout << "Candidate keys written to file " << KEYS_FILENAME << "\n";
}
