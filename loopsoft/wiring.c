#include <wiringPi.h>
#include <stdio.h>

#define RECORDING_1 15
#define RESET_1 16

#define ACTIVE_POSITION 0
#define PASSIVE_POSITION 1

int main (void) {
    // initialize wiring pi and use the simplified pin numbers 1-16
    wiringPiSetup();
    pinMode(RECORDING_1, INPUT);
    pinMode(RESET_1, INPUT);

    int value = 0;
    while(1) {
        int readvalue = digitalRead(RECORDING_1);
        if (readvalue != value) {
            printf("value of RECORDING_1 %d\n", readvalue);
            value = readvalue;
        }
    }

    return 0;
}
