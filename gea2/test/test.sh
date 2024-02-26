#!/usr/bin/env bash

# Runs all 7 test vectors automatically

cc ../gea2-enc.c

for i in {1..7} 
do
	# Redirect output to /dev/null to avoid clogging stdout
	./a.out test$i iv$i $(cat dir$i) key$i > /dev/null
done

all_pass=1

for i in {1..7} 
do
	if [ $(crc32 test$i-enc) = $(crc32 test$i-exp) ]
	then
		echo "test$i: PASSED"
	else
		echo "test$i: FAILED"
		all_pass=0
	fi
done

if [ $all_pass -eq 0 ]
then
	echo "SOME TESTS FAILED!!!"
fi
