#include <NTL/GF2.h>
#include <NTL/vec_GF2.h>
#include <NTL/mat_GF2.h>
#include "attack-parms.h"

using namespace NTL;

void companionMatrix(Mat<GF2>& M, long a);

void getInitMatrix(Mat<GF2>& M, Mat<GF2>& A, int shiftBy);

void rowUnion(Mat<GF2>& M, Mat<GF2>& A, Mat<GF2>& B);

void intersection(Mat<GF2>& M, Mat<GF2>& U, Mat<GF2>& W);

bool independent(Vec<GF2>& V, Mat<GF2>& M);

void decompose(Mat<GF2>& M, Mat<GF2>& V, Mat<GF2>& W);

void defineMatrices(Mat<GF2>& U_B, Mat<GF2>& T_AC, Mat<GF2>& V);
