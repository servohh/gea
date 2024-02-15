# GPRS Encryption Algorithm

To run the test vectors execute:

```
cc gea1-enc.c

cd test

../a.out test1 iv1 0 key1
```

Key and IV files are big-endian, i.e.: an IV file containing [0xFF, 0x00, 0x23, 0x74] gets read as 0xFF002374.
