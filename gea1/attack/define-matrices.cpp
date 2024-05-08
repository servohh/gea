#include <NTL/GF2.h>
#include <NTL/vec_GF2.h>
#include <NTL/mat_GF2.h>

#define TAPS_A 0b1011000111011001000110111011101
#define TAPS_B 0b10100010000011110000001110001111
#define TAPS_C 0b001001000101111101100111000010101

#define SHIFT_A 0
#define SHIFT_B 16
#define SHIFT_C 32

using namespace std;
using namespace NTL;

// Set M to the companion matrix of the LFSR whose taps are a
// Assume M is square and its dimensions are equal to size of LFSR
void companionMatrix(Mat<GF2>& M, long a)
{
	long rows = M.NumRows();
	Vec<GF2> x;
	x.SetLength(rows);
	for (int i = 0;i < rows;i++) {
		x[0] = conv<GF2>(a >> (rows - (i + 1)));
		for (int j = 1;j < rows;j++) {
			if ((j - 1) == i) {
				set(x[j]);
			} else {
				clear(x[j]);
			}
		}
		M[i] = x;
	}
	x.kill();
}

// Set M the transformation matrix for the initialization of the LFSR whose companion matrix is A
// Assume M has columns of size 64 and rows same size as A
void getInitMatrix(Mat<GF2>& M, Mat<GF2>& A, int shiftBy)
{
	long rows = A.NumRows();
	Vec<GF2> x;
	Vec<GF2> s;
	x.SetLength(rows);
	s.SetLength(64);
	for (int i = 0;i < 64;i++) {
		for (int j = 0;j < 64;j++) {
			if (j < rows) {
				clear(x[j]);
			}
			if (j == i) {
				set(s[j]);
			} else {
				clear(s[j]);
			}
		}
		for (int j = 0;j < 64;j++) {
			x[0] += s[(j + shiftBy) % 64];
			x = A*x;
		}
		M[i] = x;
	}
	x.kill();
	s.kill();
}

// Union of rows of two matrices. 
// Assumes number of columns to be the same, M has same number of columns and (A.NumRows() + B.NumRows()) rows
void rowUnion(Mat<GF2>& M, Mat<GF2>& A, Mat<GF2>& B)
{
	for (int i = 0;i < A.NumRows();i++) {
		M[i] = A[i];
	}
	for (int i = 0;i < B.NumRows();i++) {
		M[i + A.NumRows()] = B[i];
	}
}

// Vector space intersection with row vector basis U and W. Assumes number of columns of U and W to be the same
void intersection(Mat<GF2>& M, Mat<GF2>& U, Mat<GF2>& W)
{
	Mat<GF2> A;
	A.SetDims(U.NumRows() + W.NumRows(), U.NumCols());
	rowUnion(A, U, W);
	transpose(A, A);
	kernel(M, A);
	A.kill();
}

bool independent(Vec<GF2>& v, Mat<GF2>& M)
{
	Mat<GF2> M_add;
	M_add.SetDims(M.NumRows() + 1, M.NumCols());
	int i = 0;
	while (i < M.NumRows()) {
		M_add[i] = M[i];
		i++;
	}
	M_add[i] = v;
	long rank = gauss(M_add);
	bool ind = (rank == M_add.NumRows());
	M_add.kill();
	if (ind) return true;
	else return false;
}

// Calculate for space with basis V, subspace U where V is a direct sum of U and W
void decompose(Mat<GF2>& M, Mat<GF2>& V, Mat<GF2>& W)
{	
	int i = 0;
	for (int j = 0;j < V.NumRows();j++) {
		if (independent(V[j], W)) {
			W.SetDims(W.NumRows() + 1, W.NumCols());
			W[W.NumRows() - 1] = V[j];
			M[i] = V[j];
			i++;
		}
	}
}

void defineMatrices(Mat<GF2>& U_B, Mat<GF2>& T_AC, Mat<GF2>& V) 
{
	// Companion matrices of LFSRs
	Mat<GF2> A_Mat;
	Mat<GF2> B_Mat;
	Mat<GF2> C_Mat;
	A_Mat.SetDims(31, 31);
	B_Mat.SetDims(32, 32);
	C_Mat.SetDims(33, 33);
	companionMatrix(A_Mat, TAPS_A);
	companionMatrix(B_Mat, TAPS_B);
	companionMatrix(C_Mat, TAPS_C);

	// Transformation matrices
	Mat<GF2> M_A;
	Mat<GF2> M_B;
	Mat<GF2> M_C;
	M_A.SetDims(64, 31);
	M_B.SetDims(64, 32);
	M_C.SetDims(64, 33);
	getInitMatrix(M_A, A_Mat, SHIFT_A);
	getInitMatrix(M_B, B_Mat, SHIFT_B);
	getInitMatrix(M_C, C_Mat, SHIFT_C);
	A_Mat.kill();
	B_Mat.kill();
	C_Mat.kill();

	// Create U_B and T_AC
	//Mat<GF2> U_B;
	//Mat<GF2> T_AC;
	Mat<GF2> kernA;
	Mat<GF2> kernC;
	kernel(U_B, M_B);
	kernel(kernA, M_A);
	kernel(kernC, M_C);
	intersection(T_AC, kernA, kernC);
	kernA.kill();
	kernC.kill();

	// Create V
	//Mat<GF2> V;
	Mat<GF2> F_64;
	Mat<GF2> combineMat;
	V.SetDims(8, 64);
	combineMat.SetDims(56, 64);
	ident(F_64, 64);
	rowUnion(combineMat, U_B, T_AC);
	decompose(V, F_64, combineMat);
	combineMat.kill();
	F_64.kill();
}
