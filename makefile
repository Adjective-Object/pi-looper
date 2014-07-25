
looper: looper.c
	gcc -Wall -g -o looper looper.c -lm -lao -lpulse-simple -lpulse

test: test.c
	gcc -Wall -g -o test test.c -lm -lao -lasound
