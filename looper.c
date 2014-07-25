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
#define FRAMESIZE 1024
#define NUMFRAMES 1000
#define BUFLEN FRAMESIZE * NUMFRAMES

#define SAMPLE_BPS 44100

int main(int argc, char*argv[]) {
    /* The Sample format to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = SAMPLE_BPS,
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

    pa_usec_t inlatency = pa_simple_get_latency(ins, &error);
    pa_usec_t outlatency = pa_simple_get_latency(outs, &error);

    int netlatency = (inlatency + outlatency)/1000;
    printf("%d",netlatency*SAMPLE_BPS/((int)sizeof(int)));
    int netlatency_buf = 100; // netlatency*SAMPLE_BPS/((int)sizeof(int));

    printf("%d\n", NUMFRAMES);
    printf("%d\n", FRAMESIZE);
    printf("%d\n", BUFLEN);

    //1 byte sample size
    int *loop = malloc( sizeof(int) * BUFLEN );
    int *buf = malloc( sizeof(int) * FRAMESIZE );

    //initialize loop as zfilled
    int i;
    for(i=0; i<BUFLEN; i++){
        loop[i] = 0;
    }

    int count = 0;

    while(1) {
        /* Read some data ... */
        if (pa_simple_read(ins, buf, FRAMESIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            goto finish;
        }

        //copy into loop, accounting for latency
        for(i=0; i<FRAMESIZE; i++){
            int addr = count*FRAMESIZE + i - netlatency_buf;
            if(addr<0) {
                addr = BUFLEN + addr;
            }
            loop[addr] = loop[addr] + buf[i];
        }

        /* ... and play it */
        if (pa_simple_write(outs, loop + (count * FRAMESIZE), (size_t) FRAMESIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
            goto finish;
        }
        count = (count + 1) % NUMFRAMES;
    }

finish:
    if (ins)
        pa_simple_free(ins);
    if (outs)
        pa_simple_free(outs);
    return exitcode;
;
}