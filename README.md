# GPRS Encryption Algorithm

Key and IV files are big-endian, i.e.: an IV file containing [0xFF, 0x00, 0x23, 0x74] gets read as 0xFF002374.

## GEA-1

To run the test vectors execute:

```
cd gea1/test

./test.sh
```
