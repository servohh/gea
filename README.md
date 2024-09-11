# GPRS Encryption Algorithm

This repository contains a reference implementation of the outdated GPRS encryption algorithms GEA-1 and GEA-2 in C. It also contains an implementation of the cryptanalysis of GEA-1 described by [Beierle et al](https://eprint.iacr.org/2021/819).

Usage note: key and IV files are big-endian, i.e.: an IV file containing [0xFF, 0x00, 0x23, 0x74] gets read as 0xFF002374.

## GEA-1 and GEA-2

All code is contained in gea1/gea1-enc.c and gea2/gea2-enc.c respectively.

To run all the test vectors quickly, execute test/test-vectors/test.sh.

The code for the large keystream test is stored in gea1/test/large-keystream/large-keystream-test.c.

## GEA-1 attack

To compile both steps at the same time, simply run gea1/attack/compile.sh.

MAKE SURE YOU HAVE 48 GiB OF DISK SPACE FREE BEFORE RUNNING create-table!!!

gea1-attack will not work properly unless all tables are generated and in the working directory.

gea1-attack also requires keystream file, IV file and dir (0 or 1) as parameters.
