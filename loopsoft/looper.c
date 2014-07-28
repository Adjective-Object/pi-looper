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

int keyrecording = 0;
SDL_Joystick *joy = NULL;

int getkey() {
    int character;
    struct termios orig_term_attr;
    struct termios new_term_attr;

    /* set the terminal to raw mode */
    tcgetattr(fileno(stdin), &orig_term_attr);
    memcpy(&new_term_attr, &orig_term_attr, sizeof(struct termios));
    new_term_attr.c_lflag &= ~(ECHO|ICANON);
    new_term_attr.c_cc[VTIME] = 0;
    new_term_attr.c_cc[VMIN] = 0;
    tcsetattr(fileno(stdin), TCSANOW, &new_term_attr);

    /* read a character from the stdin stream without blocking */
    /*   returns EOF (-1) if no character is available */
    character = fgetc(stdin);

    /* restore the original terminal attributes */
    tcsetattr(fileno(stdin), TCSANOW, &orig_term_attr);

    return character;
}

int oldrecording = 0;
int isRecording(){
    if( joy == NULL ){
        if (getkey() != -1){
            keyrecording = !keyrecording;
        }
        if(keyrecording != oldrecording){
            oldrecording = keyrecording;
            printf("\nrecording: %d\n", oldrecording);
        }
        return oldrecording;
    } else {
        SDL_JoystickUpdate();
        if (SDL_JoystickGetButton(joy, 0) != oldrecording){
            oldrecording = SDL_JoystickGetButton(joy,0);
            printf("\nrecording: %d\n", oldrecording);
        }
        return oldrecording;
    }
}



int main(int argc, char*argv[]) {
    int i;


    /* The Sample format to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = SAMPLE_HZ,
        .channels = 2
    };
    pa_simple *outs = NULL;
    pa_simple *ins = NULL;
    int exitcode = 1;
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
        goto finish;
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
        goto finish;
    }

    printf("%d\n", MAXNUMFRAMES);
    printf("%d\n", FRAMESIZE);
    printf("%d\n", BUFLEN);


    /* setup controller support */
    if (SDL_Init( SDL_INIT_JOYSTICK ) < 0){
        fprintf(stderr, "Couldn't initialize SDL: %s\n", SDL_GetError());
        exit(1);
    }
    printf("%i joysticks were found.\n\n", SDL_NumJoysticks() );

    if(SDL_NumJoysticks() > 0){
        printf("using joystick '%s'\n", SDL_JoystickName(0));
        joy = SDL_JoystickOpen(0);
    } else{
        printf("no joysticks found, please use keyboard controls\n");
    }

    /* setup buffers */
    int *loop = malloc( sizeof(int) * BUFLEN );
    int *inbuf = malloc( sizeof(int) * FRAMESIZE );

    printf("waiting for input\n");
    while(!isRecording()) {
        //clear the contents of the buffer
        pa_simple_read(ins, inbuf, FRAMESIZE, &error);
    }

    printf("starting initial recording\n");

    //initial recording
    int looplen;
    for(looplen = 0; looplen<MAXNUMFRAMES; looplen++){
        if(pa_simple_read(ins, loop+looplen*FRAMESIZE, FRAMESIZE, &error) < -1){
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            goto finish;
        }
        if(!isRecording()) {
            break;
        }
    }

    int LOOPLENN = looplen * FRAMESIZE;
    printf("looplen %d\n", looplen);
    
    int netlatency = pa_simple_get_latency(ins,&error) + pa_simple_get_latency(ins,&error);
    int addtl_latency_usec = 18000;

    //bytes to shift incoming audio
    int latency = (netlatency+addtl_latency_usec) / 1000000 * SAMPLE_HZ / FRAMESIZE * FRAMESIZE; // make divisible by 32

    int count = 0;
    while(1) {
        int current_head = count*FRAMESIZE;

        /* Read some data into the buffer */
        if (pa_simple_read(ins, inbuf, FRAMESIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            goto finish;
        }

        /* add into loop_record at the record head */
        if(isRecording()) {
            for(i=0; i<FRAMESIZE; i++){
                long addr = (current_head + i - latency) % LOOPLENN;
                if(addr<0) {
                    addr = LOOPLENN + (addr % LOOPLENN);
                }

                loop[addr] = loop[addr] + inbuf[i];
            }
        }

        /* play the appropriate part of loop_play */
        if (pa_simple_write(outs, loop + current_head, (size_t) FRAMESIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
            goto finish;
        }

        /* increment count for next loop */
        count = (count + 1) % looplen;
        if(count == 0){
            printf("."); fflush(stdout);
        }
    }

finish:
    if (ins)
        pa_simple_free(ins);
    if (outs)
        pa_simple_free(outs);
    return exitcode;
;
}