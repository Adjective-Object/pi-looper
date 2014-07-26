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
#include <time.h>
#define FRAMESIZE 1024
#define MAXNUMFRAMES 3000
#define BUFLEN FRAMESIZE * MAXNUMFRAMES

#define SAMPLE_HZ 44100

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

int main(int argc, char*argv[]) {
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

    int *loop = malloc( sizeof(int) * BUFLEN );
    int *buf = malloc( sizeof(int) * FRAMESIZE );

    //

    //initial recording
    int looplen;
    for(looplen = 0; looplen<MAXNUMFRAMES; looplen++){
        if (pa_simple_read(ins, loop + (looplen * FRAMESIZE), FRAMESIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            goto finish;
        }
        if(getkey() != -1){
            break;
        }
    }

    printf("looplen %d\n", looplen);


    pa_usec_t outlatency;
    pa_usec_t inlatency;
    int count = 0;
    while(1) {
        if( (inlatency = pa_simple_get_latency(ins, &error)) < 0 ){
            fprintf(stderr, __FILE__": pa_simple_get_latency() failed: %s\n", pa_strerror(error));
        }
        if( (outlatency = pa_simple_get_latency(ins, &error)) < 0 ){
            fprintf(stderr, __FILE__": pa_simple_get_latency() failed: %s\n", pa_strerror(error));
        }
        int netlatency_buf = (inlatency + outlatency) * SAMPLE_HZ / 1000000 / BUFLEN + 10; // netlatency*SAMPLE_BPS/((int)sizeof(int));
        
        /* Read some data ... */
        if (pa_simple_read(ins, buf, FRAMESIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            goto finish;
        }

        //copy into loop, accounting for latency
        int i;
        for(i=0; i<FRAMESIZE; i++){
            int addr = count*FRAMESIZE - netlatency_buf + i;
            if(addr<0) {
                addr = looplen + (addr % looplen);
            }
            loop[addr] = loop[addr] + buf[i];
        }

        /* ... and play it */
        if (pa_simple_write(outs, loop + (count * FRAMESIZE), (size_t) FRAMESIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
            goto finish;
        }
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