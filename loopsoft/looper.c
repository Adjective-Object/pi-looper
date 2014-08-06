/***
  This file is part of PulseAudio.
  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.
  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.
  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.
***/
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pulse/simple.h>
#include <pulse/sample.h>
#include <pulse/error.h>
#include <termios.h>
#include <SDL/SDL.h>
#include <time.h>
#define FRAMESIZE 32
#define MAXNUMFRAMES 30000
#define BUFLEN FRAMESIZE * MAXNUMFRAMES

#define SAMPLE_HZ 44100
#define NUM_LOOPS 3

int getkey() {
    int character;

    /* read a character from the stdin stream without blocking */
    /*   returns EOF (-1) if no character is available */
    character = fgetc(stdin);

    return character;
}

int keyrecording = 0;
int oldrecording = 0;
SDL_Joystick *joy = NULL;


struct recordingloop{
    //pointer to end of loop
    int *body;
    //point to overwrite until. Used for efficient live reset
    //-1 indicates no overwrite
    short resetpoint;
    //recording
    short recording;
};


int recordingPressed = 0;
int resetPressed = 0;

void doInput(struct recordingloop subloops[], int currenttime){
    int k;
    int changed = 0;
    resetPressed = 0;

    if(joy == NULL){
        while((k = getkey()) != -1){
            //update this when update num channels
            switch(k) {
                case '1':
                    recordingPressed = recordingPressed ^ 1; //0001
                    break;
                case '2':
                    recordingPressed = recordingPressed ^ 2; //0010
                    break;
                case '3':
                    recordingPressed = recordingPressed ^ 4; //0100
                    break;

                case 'q':
                    resetPressed = resetPressed ^ 1;
                    break;
                case 'w':
                    resetPressed = resetPressed ^ 2;
                    break;
                case 'e':
                    resetPressed = resetPressed ^ 4;
                    break;
            }
            changed = 1;
        }
    }
    else{
        recordingPressed = 0;
        SDL_JoystickUpdate(); //TODO test this shit
        for (k=0; k<NUM_LOOPS; k++){
            if (SDL_JoystickGetButton(joy, k)){
                recordingPressed = recordingPressed ^ (1<<k);
                changed = 1;
            }

            if (SDL_JoystickGetButton(joy, k+NUM_LOOPS)){
                resetPressed = resetPressed ^ (1<<k);
                changed = 1;
            }
        }
    }

    //printf("%d, %d\n", recordingPressed, resetPressed);

    for(k=0; k<NUM_LOOPS; k++){
        ///printf("(%d, %d) ", k, NUM_LOOPS);
        subloops[k].recording = (recordingPressed>>k) & 1;
        if ( (resetPressed>>k) & 1){

            subloops[k].resetpoint = currenttime;
        }
    }

    if(changed){
        /*
        printf("\n(%d %d %d) (%d %d %d)\n",
            recordingPressed & 1,
            (recordingPressed>>1) & 1,
            (recordingPressed>>2) & 1,
            resetPressed & 1,
            (resetPressed>>1) & 1,
            (resetPressed>>2) & 1);
        */
        printf("\n(%d %d %d) (%d %d %d) ",
            subloops[0].recording,
            subloops[1].recording,
            subloops[2].recording,
            subloops[0].resetpoint,
            subloops[1].resetpoint,
            subloops[2].resetpoint);
    }
}

int anyRecording(struct recordingloop subloops[]){
    int i;
    for(i=0; i<NUM_LOOPS; i++){
        if(subloops[i].recording){
            return 1;
        }
    }
    return 0;
}

int anyReset(struct recordingloop subloops[]){
    int i;
    for(i=0; i<NUM_LOOPS; i++){
        if(subloops[i].resetpoint != -1){
            return 1;
        }
    }
    return 0;
}

pa_simple *outs = NULL;
pa_simple *ins = NULL;
int exitcode = 1;

struct termios orig_term_attr;
struct termios new_term_attr;

void finish(){

    /* restore the original terminal attributes */
    tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);

    if (ins)
        pa_simple_free(ins);
    if (outs)
        pa_simple_free(outs);
    exit(exitcode);
}

void handleReadin(struct recordingloop subloops[],  
                int *inbuf, 
                int latency,
                int LOOPLENN, 
                int current_head){
    int i;
    int x;

    for(i=0; i<FRAMESIZE; i++){
        //calc addr
        long addr = (current_head + i - latency) % LOOPLENN;    
        if (addr<0) {
            addr = LOOPLENN + (addr % LOOPLENN);
        }

        for (x=0; x<NUM_LOOPS; x++){
            if(subloops[x].recording){
                //if this track has not been reset, copy the new data in
                if (subloops[x].resetpoint == -1){
                    subloops[x].body[addr] = subloops[x].body[addr] + inbuf[i];
                }
                //otherwise, move direct overwrite if recording
                else{
                    subloops[x].body[addr] = inbuf[i];
                }
            }
            //otherwise, if it has been reset and not recording, set 0.
            else if (subloops[x].resetpoint != -1) {
                subloops[x].body[addr] = 0;
            }
        }
    }
}

int main(int argc, char*argv[]) {

    /* set the terminal to raw mode */
    tcgetattr(fileno(stdin), &orig_term_attr);
    memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
    new_term_attr.c_lflag &= ~(ECHO|ICANON);
    new_term_attr.c_cc[VTIME] = 0;
    new_term_attr.c_cc[VMIN] = 0;
    tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

    //iterator variables
    int i;
    int x;


    /* The Sample format to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = SAMPLE_HZ,
        .channels = 2
    };
    int error;

    /* Create a new playback stream */
    if ( !( outs = pa_simple_new(
            NULL, // server
            "pi-looper", // name
            PA_STREAM_PLAYBACK, // direction
            NULL, // dev?
            "playback", //  stream_name
            &ss, // sample format
            NULL, // default channel map
            NULL, //deault buffering
            &error //error code to error
        ) )) {

        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        finish();
    }

    /* Create a new capture stream */
    if ( !( ins = pa_simple_new(
            NULL, // server
            "pi-looper", // name
            PA_STREAM_RECORD, // direction
            NULL, // dev?
            "record", //  stream_name
            &ss, // sample format
            NULL, // default channel map
            NULL, //deault buffering
            &error //error code to error
        ) )) {

        fprintf(stderr, __FILE__": pa_simple_new() failed: %s\n", pa_strerror(error));
        finish();
    }
    printf("\n");
    /*
    printf("%d\n", MAXNUMFRAMES);
    printf("%d\n", FRAMESIZE);
    printf("%d\n", BUFLEN);
    */

    /* setup controller support */
    if (SDL_Init( SDL_INIT_JOYSTICK ) < 0){
        fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }
    printf("%i joysticks were found.\n", SDL_NumJoysticks() );

    if(SDL_NumJoysticks() > 0){
        printf("using joystick '%s'\n\n", SDL_JoystickName(0));
        joy = SDL_JoystickOpen(0);
    } else{
        printf("please use keyboard controls\n\n");
    }
    
    int netlatency = pa_simple_get_latency(ins,&error) + pa_simple_get_latency(ins,&error);
    int addtl_latency_usec = 18000;

    //bytes to shift incoming audio
    int latency = (netlatency+addtl_latency_usec) / 1000000 * SAMPLE_HZ / FRAMESIZE * FRAMESIZE; // make divisible by 32

    /* setup buffers */
    int *masterloop = malloc( sizeof(int) * BUFLEN );
    int *inbuf = malloc( sizeof(int) * FRAMESIZE );

    /* initialize 3 midbuffers */
    struct recordingloop subloops[NUM_LOOPS];
    for (i=0; i<NUM_LOOPS; i++){
        subloops[i].body = malloc(sizeof(int) * BUFLEN);
        subloops[i].recording = 0;
        subloops[i].resetpoint = -1;
    }

    doInput(subloops, -1);
    
    printf("Start recording on any channel to begin\n");
    while( !anyRecording(subloops) ) {
        doInput(subloops, -1);
        //clear the contents of the buffer
        pa_simple_read(ins, inbuf, FRAMESIZE, &error);
    }

    printf("starting initial recording\n");

    //initial recording
    int looplen;
    for(looplen = 0; looplen<MAXNUMFRAMES; looplen++){
        //TODO actually do channeled read
        if (pa_simple_read(ins, inbuf, FRAMESIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            finish();
        }

        handleReadin(subloops, inbuf, 0, BUFLEN, looplen * FRAMESIZE);

        doInput(subloops, looplen);
        if(!anyRecording(subloops)) {
            break;
        }
    }

    int LOOPLENN = looplen * FRAMESIZE;
    printf("looplen %d\n", looplen);

    int count = 0;
    int current_head;
    while(1) {
        current_head = count*FRAMESIZE;

        /* Read some data into the buffer */
        if (pa_simple_read(ins, inbuf, FRAMESIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            finish();
        }

        doInput(subloops, count);

        if( anyRecording(subloops) || anyReset(subloops) ) {
            handleReadin(subloops, inbuf, latency, LOOPLENN, current_head);
        }
        
        for(i=current_head; i<current_head+FRAMESIZE; i++){
            int sum = 0;
            for (x=0; x<NUM_LOOPS; x++){
                //if not reset, copy into masterloop.
                if (subloops[x].resetpoint == -1){
                    sum = sum + subloops[x].body[i];
                }
            }
            masterloop[i] = sum;
        }

        /* play the appropriate part of loop_play */
        if (pa_simple_write(outs, masterloop + current_head, (size_t) FRAMESIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
            finish();
        }

        /* increment count for next loop */
        count = (count + 1) % looplen;

        /* reset subloop resetpoints if appropriate */
        for (i=0; i<NUM_LOOPS; i++){
            if (subloops[i].resetpoint == count){
                printf("\nreset channel (%d) complete", i);
                subloops[i].resetpoint = -1;
            }
        }

        if(count == 0){
            printf("."); fflush(stdout);
        }
    }
}

