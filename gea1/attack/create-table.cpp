#include <NTL/GF2.h>
#include <NTL/vec_GF2.h>
#include <NTL/mat_GF2.h>
#include <NTL/BasicThreadPool.h>
#include <fstream>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include "define-matrices.h"
#include "gea1-func.h"

using namespace NTL;
using namespace std;

// Generate a vector from the space spanned by A, using l bits of n as scalars
// e.g.: if A consists of vectors [v1, v2, v3] and l = 0b010, returns V = (0 * v1) + (1 * v2) + (0 * v3)
void span(Vec<GF2>& v, Mat<GF2>& A, long n, int l)
{
	Vec<GF2> multVec;
	for (int i = (l - 1);i >= 0;i--) {
		multVec.append(conv<GF2>((n >> i) % 2));
	}
	v = transpose(A) * multVec;
	multVec.kill();
}

// Quicksort algorithm on table Tab[v]
void sort(unsigned char* tab, long left, long right)
{
	if (left >= right | left < 0) {
		return;
	}
	
	// Partition
	unsigned char* pivot = tab + left; // Choose leftmost element as pivot.
	long i = left - ENTRY_SIZE;
	long j = right + ENTRY_SIZE;	
	while (true) {
		do {
			i += ENTRY_SIZE;
		} while (memcmp(tab + i + 3, pivot + 3, (CONST_L/8)*sizeof(unsigned char)) < 0);
		do {
			j -= ENTRY_SIZE;
		} while (memcmp(tab + j + 3, pivot + 3, (CONST_L/8)*sizeof(unsigned char)) > 0);
		if (i >= j) break;
		unsigned char* temp2 = (unsigned char*) malloc(ENTRY_SIZE * sizeof(unsigned char));
		memcpy(temp2, tab + i, ENTRY_SIZE * sizeof(unsigned char));
		memcpy(tab + i, tab + j, ENTRY_SIZE * sizeof(unsigned char));
		memcpy(tab + j, temp2, ENTRY_SIZE * sizeof(unsigned char));
		free(temp2);
	}

	// Sort the partitions
	sort(tab, left, j);
	sort(tab, j + ENTRY_SIZE, right);
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

int main()
{
	SetNumThreads(NUM_THREADS);
	
	Mat<GF2> U_B;
	Mat<GF2> T_AC;
	Mat<GF2> V;
	defineMatrices(U_B, T_AC, V);

	Mat<GF2> B_Mat;
	B_Mat.SetDims(32, 32);
	companionMatrix(B_Mat, TAPS_B);
	Mat<GF2> M_B;
	M_B.SetDims(64, 32);
	getInitMatrix(M_B, B_Mat, SHIFT_B);
	transpose(M_B, M_B);

	NTL_EXEC_RANGE(SIZE_V, first, last)
	for (long i = first;i < last;i++) {
		Vec<GF2> vec_v;
		Vec<GF2> vec_t;
		Vec<GF2> B_tv;
		unsigned char* tab_v = (unsigned char*) malloc((ENTRY_SIZE * SIZE_TAC) * sizeof(unsigned char));
		span(vec_v, V, i, 8);
		char* string_i = (char*) malloc((i % 10) * sizeof(char));
		sprintf(string_i, "%li", i);
		int tabIndex = 0;	
		for (long j = 0;j < SIZE_TAC;j++) {
			span(vec_t, T_AC, j, 24);
			cout << vec_t << "\n";
			for (int a = 0;a < 3;a++) {
				tab_v[tabIndex + a] = (unsigned char) (j >> ((2 - a) * 8));
			}
			B_tv = M_B * (vec_t + vec_v);
			struct lfsr B = getLFSR(B_tv, 0b11110001110000001111000001000101);
			if (B.state == 0) B.state = 1;
			generateOutputB(B, tab_v + tabIndex + 3);
			tabIndex += ENTRY_SIZE;
		}
		sort(tab_v, 0, ENTRY_SIZE * ((SIZE_TAC) - 1));	
		ofstream table(string_i, ios::out | ios::binary);
		for (int i = 0;i < (ENTRY_SIZE * SIZE_TAC);i++) {
			table << tab_v[i];
		}
		table.close();
		vec_v.kill();
		vec_t.kill();
		B_tv.kill();
		free(tab_v);
		free(string_i);
	}
	NTL_EXEC_RANGE_END	
}
