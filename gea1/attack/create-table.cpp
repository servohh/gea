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
#define TAPS_B 0b10100010000011110000001110001111
#define SHIFT_B 16

#define SIZE_V 1 << 8
#define SIZE_TAC 1 << 24
#define SIZE_UB 1 << 32

#define NUM_THREADS 16

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

// GEA-1 nonlinear function
GF2 f(GF2 x0, GF2 x1, GF2 x2, GF2 x3, GF2 x4, GF2 x5, GF2 x6)
{
        return x0+x2+x5+x6 * x0+x3+x5+x6 * x0+x1+x5+x6 * x1+x2+x5+x6 * x0+x2+x3+x6 * x1+x3+x4+x6 * x1+x3+x5+x6 * x0+x2+x4 * x0+x2+x3 * x0+x1+x3 * x0+x2+x6 * x0+x1+x4 * x0+x1+x6 * x1+x2+x6 * x2+x5+x6 * x0+x3+x5 * x1+x4+x6 * x1+x2+x5 * x0+x3 * x0+x5 * x1+x3 * x1+x5 * x1+x6 * x0+x2 * x1 * x2+x3 * x2+x5 * x2+x6 * x4+x5 * x5+x6 * x2 * x3 * x5;
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
			for (int a = 0;a < 3;a++) {
				tab_v[tabIndex + a] = (unsigned char) (j >> ((2 - a) * 8));
			}
			B_tv = M_B * (vec_t + vec_v);
			GF2 outBit;
			int out = 0;
			int outLength = 0;
			int index_b = 3;
			for (int k = 0;k < CONST_L;k++) {
				outBit = f(B_tv[12], B_tv[27], B_tv[0], B_tv[1], B_tv[29], B_tv[21], B_tv[5]);
				B_tv = B_Mat * B_tv;
				out <<= 1;
				out += IsOne(outBit);
				outLength++;
				if (outLength == 8) {
					tab_v[tabIndex + index_b] = out;
					index_b++;
					outLength = 0;
					out = 0;
				}
			}
			if (out != 0) {
				tab_v[tabIndex + index_b] = out;
			}
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
