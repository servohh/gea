const_l = 72

# Get the transformation matrix for the initialization of LFSR l
def getInitMatrix(l, shiftBy):
	P.<x> = PolynomialRing(GF(2))
	size = l.degree()
	A = companion_matrix(l, "left") # Companion matrix of l
	M_l = zero_matrix(GF(2), 64, size)
	for i in range(64):
		x = zero_vector(GF(2), size)
		s = zero_vector(GF(2), 64)
		s[i] = 1 # Create basis of F(2, 64)
		for j in range(64): # Perform initialization
			x[0] = x[0] + s[(j + shiftBy) % 64] # Shifted insertion
			x = A*x # Clock LFSR
		M_l[i] = x
	return M_l

# GEA nonlinear function
def f_nonlinear(x0, x1, x2, x3, x4, x5, x6):
	return x0+x2+x5+x6 * x0+x3+x5+x6 * x0+x1+x5+x6 * x1+x2+x5+x6 * x0+x2+x3+x6 * x1+x3+x4+x6 * x1+x3+x5+x6 * x0+x2+x4 * x0+x2+x3 * x0+x1+x3 * x0+x2+x6 * x0+x1+x4 * x0+x1+x6 * x1+x2+x6 * x2+x5+x6 * x0+x3+x5 * x1+x4+x6 * x1+x2+x5 * x0+x3 * x0+x5 * x1+x3 * x1+x5 * x1+x6 * x0+x2 * x1 * x2+x3 * x2+x5 * x2+x6 * x4+x5 * x5+x6 * x2 * x3 * x5

# Compare function for sorting table
def cmp_l(l1, l2):
	for i in range(len(l1[1])):
		diff = l1[1][i] - l2[1][i]
		if diff == 0:
			continue
		else:
			return diff
	return 0

P.<x> = PolynomialRing(GF(2))
# Polynomial forms of LFSRs
A_poly = x^31 + x^30 + x^28 + x^27 + x^23 + x^22 + x^21 + x^19 + x^18 + x^15 + x^11 + x^10 + x^8 + x^7 + x^6 + x^4 + x^3 + x^2 + 1
B_poly = x^32 + x^31 + x^29 + x^25 + x^19 + x^18 + x^17 + x^16 + x^9 + x^8 + x^7 + x^3 + x^2 + x + 1
C_poly = x^33 + x^30 + x^27 + x^23 + x^21 + x^20 + x^19  + x^18 + x^17 + x^15 + x^14 + x^11 + x^10 + x^9 + x^4 + x^2 + 1
A_shift = 0
B_shift = 16
C_shift = 32

M_A = getInitMatrix(A_poly, A_shift)
M_B = getInitMatrix(B_poly, B_shift)
M_C = getInitMatrix(C_poly, C_shift)

# Get U_B, T_AC and V
U_B = M_B.kernel()
T_AC = M_A.kernel().intersection(M_C.kernel())
V = (U_B + T_AC).complement()

# Make table
B_comp = companion_matrix(B_poly, "left")
V_list = V.list()
T_list = T_AC.list()
lFile = open('l', 'w')
lFile.write(str(const_l))
lFile.close()
counter = 0
for v in V_list:
	tabEntry = []
	for t in T_list:
		count2 = 0
		t_int = 0
		for i in range(len(t)):
			t_int += (2**(63 - i)) * int(t[i])
		B_tv = M_B.transpose() * (t + v)
		out = 0
		outLength = 0
		outList = []
		for i in range(const_l):
			outBit = f_nonlinear(int(B_tv[12]), int(B_tv[27]), int(B_tv[0]), int(B_tv[1]), int(B_tv[29]), int(B_tv[21]), int(B_tv[5]))
			B_tv = B_comp * B_tv
			out <<= 1
			out += outBit
			outLength += 1
			if outLength % 8 == 0:
				outList.append(out)
				out = 0
		if out != 0:
			outList.append(out)
		tabEntry.append([t_int, outList]) 
		count2 += 1
		if count2 % 100 == 0:
			print(f'Added {count2:d} entries')
	print(f'Writing to tab{counter:d}')
	f = open(f'tab{counter:d}', 'wb')
	for i in tabEntry.sorted(key=cmp_to_key(cmp_l)):
		f.write(tabEntry[0].toBytes(3, 'big'))
		for j in tabEntry[1]:
			f.write(j.toBytes(1, 'big'))
	f.close()
	counter += 1
