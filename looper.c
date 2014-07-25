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
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#define BUFSIZE 1024

int main(int argc, char*argv[]) {
    /* The Sample format to use */
    static const pa_sample_spec ss = {
        .format = PA_SAMPLE_S16LE,
        .rate = 44100,
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

    uint8_t buf[BUFSIZE];
    for (;;) {

        /* Read some data ... */
        if (pa_simple_read(ins, buf, (size_t) BUFSIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
            goto finish;
        }
        /* ... and play it */
        if (pa_simple_write(outs, buf, (size_t) BUFSIZE, &error) < 0) {
            fprintf(stderr, __FILE__": pa_simple_write() failed: %s\n", pa_strerror(error));
            goto finish;
        }
    }

    /* Make sure that every single sample was played */
    if (pa_simple_drain(outs, &error) < 0) {
        fprintf(stderr, __FILE__": pa_simple_drain() failed: %s\n", pa_strerror(error));
        goto finish;
    }
    
    exitcode = 0;


finish:
    if (ins)
        pa_simple_free(ins);
    if (outs)
        pa_simple_free(outs);
    return exitcode;
;
}