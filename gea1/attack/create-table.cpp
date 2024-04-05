#include <NTL/GF2.h>
#include <NTL/vec_GF2.h>
#include <NTL/mat_GF2.h>

#define CONST_L 72

#define TAPS_A 0b1011101110110001001101110001101
#define TAPS_B 0b11110001110000001111000001000101
#define TAPS_C 0b101010000111001101111101000100100

#define SHIFT_A 0
#define SHIFT_B 16
#define SHIFT_C 32

using namespace std;
using namespace NTL;

// Set M to the companion matrix of the LFSR whose taps are a
// Assume M is square and its dimensions are equal to size of LFSR
Mat<GF2> companionMatrix(Mat<GF2>& M, long a)
{
	long size = M.NumRows();
	Vec<GF2> x;
	x.SetLength(size);
	for (int i = 0;i < size;i++) {
		x[0] = conv<GF2>(a >> (size - (i + 1)) == 0);
		for (int j = 1;j < size;j++) {
			if ((j - 1) == i) {
				set(x[j]);
			} else {
				clear(x[j]);
			}
		}
		M[i] = x;
	}
	return M;
}

// Set M the transformation matrix for the initialization of the LFSR whose companion matrix is A
// Assume M has columns of size 64 and rows same size as A
Mat<GF2> getInitMatrix(Mat<GF2>& M, Mat<GF2>& A, int shiftBy)
{
	long size = A.NumRows();
	Vec<GF2> x;
	Vec<GF2> s;
	x.SetLength(size);
	s.SetLength(64);
	for (int i = 0;i < 64;i++) {
		for (int j = 0;j < 64;j++) {
			if (j < size) {
				clear(x[j]);
			}
			if (j == i) {
				set(s[j]);
			} else {
				set(s[j]);
			}
		}
		for (int j = 0;j < 64;j++) {
			x[0] += s[(j + shiftBy) % 64];
			x = A*x;
		}
		M[i] = x;
	}
	return M;
}

unsigned char f(unsigned char x0, unsigned char x1, unsigned char x2, unsigned char x3, unsigned char x4, unsigned char x5, unsigned char x6) 
{
	return x0&x2&x5&x6 ^ x0&x3&x5&x6 ^ x0&x1&x5&x6 ^ x1&x2&x5&x6 ^ x0&x2&x3&x6 ^ x1&x3&x4&x6 ^ x1&x3&x5&x6 ^ x0&x2&x4 ^ x0&x2&x3 ^ x0&x1&x3 ^ x0&x2&x6 ^ x0&x1&x4 ^ x0&x1&x6 ^ x1&x2&x6 ^ x2&x5&x6 ^ x0&x3&x5 ^ x1&x4&x6 ^ x1&x2&x5 ^ x0&x3 ^ x0&x5 ^ x1&x3 ^ x1&x5 ^ x1&x6 ^ x0&x2 ^ x1 ^ x2&x3 ^ x2&x5 ^ x2&x6 ^ x4&x5 ^ x5&x6 ^ x2 ^ x3 ^ x5;
}

int main() 
{
	// Companion matrices of LFSRs
	Mat<GF2> A_Mat;
	Mat<GF2> B_Mat;
	Mat<GF2> C_Mat;
	A_Mat.SetDims(31, 31);
	B_Mat.SetDims(32, 32);
	C_Mat.SetDims(33, 33);
	A_Mat = companionMatrix(A_Mat, TAPS_A);
	B_Mat = companionMatrix(B_Mat, TAPS_B);
	C_Mat = companionMatrix(C_Mat, TAPS_C);
	for (int i = 0;i < 31;i++) {
		for (int j = 0;j < 31;j++) {
			cout << A_Mat[i][j] << " ";
		}
		cout << "\n";
	}

	// Transformation matrices
	Mat<GF2> M_A;
	Mat<GF2> M_B;
	Mat<GF2> M_C;
	M_A.SetDims(64, 31);
	M_B.SetDims(64, 32);
	M_C.SetDims(64, 33);
	M_A = getInitMatrix(M_A, A_Mat, SHIFT_A);
	M_B = getInitMatrix(M_B, B_Mat, SHIFT_B);
	M_C = getInitMatrix(M_C, C_Mat, SHIFT_C);

	// Create U_B and T_AC
	Mat<GF2> U_B;
	Mat<GF2> T_AC;
	Mat<GF2> kernA;
	Mat<GF2> kernC;
	Mat<GF2> kernA_T;
	Mat<GF2> kernC_T;
	Mat<GF2> T_AC_T;
	kernel(U_B, M_B);
	kernel(kernA, M_A);
	kernel(kernC, M_C);
	transpose(kernA_T, kern_A);
	transpose(kernC_T, kern_C);
	int numRows = 0;
	T_AC_T.SetDims(numRows, kernA.NumRows());
	for (int i = 0;i < kernC.NumColumns();i++) {
		for (int j = 0;i < kernA.NumColumns();i++) {
			if (kernC_T[i] == kernA_T[j]) {
				numRows++;
				T_AC_T.SetDims(numRows, kernA.NumRows());
				T_AC_T[numRows - 1] = kernC_T[i];
			}
		}
	}
	transpose(T_AC, T_AC_T);
	kernA.kill()
	kernC.kill()
	kernA_T.kill()
	kernC_T.kill()
	T_AC_T.kill()
}
