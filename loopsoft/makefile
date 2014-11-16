all: looper test wiring

looper: looper.c
	gcc -Wall -g -o looper looper.c -lm -lao -lpulse-simple -lpulse -lwiringPi


test: test.c
	gcc -Wall -g -o test test.c -lm -lao -lasound 

wiring: wiring.c
	gcc -Wall -g -o wiring wiring.c -lwiringPi 
