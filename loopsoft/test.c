#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <poll.h>
#include <alsa/asoundlib.h>
#pragma GCC diagnostic ignored "-Wuninitialized"

uint rate = 170400;

void setup_channel(snd_pcm_t **handle, snd_pcm_hw_params_t **hw_params){
    int err;

    printf("."); fflush(stdout);

    if ((err = snd_pcm_hw_params_malloc (hw_params)) < 0) {
        fprintf (stderr, "cannot allocate hardware parameter structure (%s)\n",
             snd_strerror (err));
        exit (1);
    }
    
    printf("."); fflush(stdout);

    if ((err = snd_pcm_hw_params_any (*handle, *hw_params)) < 0) {
        fprintf (stderr, "cannot initialize hardware parameter structure (%s)\n",
             snd_strerror (err));
        exit (1);
    }

    printf("."); fflush(stdout);

    if ((err = snd_pcm_hw_params_set_access (*handle, *hw_params, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
        fprintf (stderr, "cannot set access type (%s)\n",
             snd_strerror (err));
        exit (1);
    }

    printf("."); fflush(stdout);

    if ((err = snd_pcm_hw_params_set_format (*handle, *hw_params, SND_PCM_FORMAT_S16_LE)) < 0) {
        fprintf (stderr, "cannot set sample format (%s)\n",
             snd_strerror (err));
        exit (1);
    }

    printf("."); fflush(stdout);

    int dir = 0;
    printf("\n%d\n", rate);

    if ((err = snd_pcm_hw_params_set_rate_near (*handle, *hw_params, &rate, &dir)) < 0) {
        fprintf (stderr, "cannot set sample rate (%s)\n",
             snd_strerror (err));
        exit (1);
    }

    printf("%d\n", rate);

    printf("."); fflush(stdout);

    if ((err = snd_pcm_hw_params_set_channels (*handle, *hw_params, 2)) < 0) {
        fprintf (stderr, "cannot set channel count (%s)\n",
             snd_strerror (err));
        exit (1);
    }

    printf("."); fflush(stdout);

    if ((err = snd_pcm_hw_params (*handle, *hw_params)) < 0) {
        fprintf (stderr, "cannot set parameters (%s)\n",
             snd_strerror (err));
        exit (1);
    }

    printf(".\n"); fflush(stdout);

    snd_pcm_hw_params_free (*hw_params);

    if ((err = snd_pcm_prepare (*handle)) < 0) {
        fprintf (stderr, "cannot prepare audio interface for use (%s)\n",
             snd_strerror (err));
        exit (1);
    }
}

void setup_cap(snd_pcm_t **handle, snd_pcm_hw_params_t **hw_params){
    printf("."); fflush(stdout);

    int rc;

    if ((rc = snd_pcm_open (handle, "default", SND_PCM_STREAM_CAPTURE, 0)) < 0) {
        fprintf (stderr, "cannot open audio device %s (%s)\n", 
             "default",
             snd_strerror (rc));
        exit (1);
    }

    setup_channel(handle, hw_params);

}

void setup_play(snd_pcm_t **playback_handle, snd_pcm_hw_params_t **hw_params){
    printf("."); fflush(stdout);

    int err;

    if ((err = snd_pcm_open (playback_handle, "default", SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
        fprintf (stderr, "cannot open audio device %s (%s)\n", 
             "default",
             snd_strerror (err));
        exit (1);
    }

    setup_channel(playback_handle, hw_params);

}


int main (int argc, char *argv[]) {
    printf("setup\n");
    fflush(stdout);

    //alsa setup
    snd_pcm_t *capture_handle;
    snd_pcm_hw_params_t *hw_params_cap;
    setup_cap(&capture_handle, &hw_params_cap);

    snd_pcm_t *play_handle;
    snd_pcm_hw_params_t *hw_params_play;
    setup_play(&play_handle, &hw_params_play);


    printf("%d %d\n",
        snd_pcm_start(capture_handle),
        snd_pcm_start(play_handle)
        ); 

    printf("!!");

    snd_pcm_uframes_t frames;
    int dir ;

    printf("!!");

    /* Use a buffer large enough to hold one period */
    snd_pcm_hw_params_get_period_size(hw_params_cap, &frames, &dir);

    /* 2 bytes/sample, 2 channels */
    uint size = frames * 4;
    char *buffer = (char *) malloc(size);
    int rc;
    uint loop = 5000000;

    printf("!!");

    while(loop > 0){
        loop --;

        /*load sound in*/
        rc = snd_pcm_readi(capture_handle, buffer, frames);
        if (rc == -EPIPE) {
          /* EPIPE means overrun */
          fprintf(stderr, "overrun occurred\n");
          snd_pcm_prepare(capture_handle);
        } else if (rc < 0) {
          fprintf(stderr,
                  "error from read: %s\n",
                  snd_strerror(rc));
        } else if (rc != (int)frames) {
          fprintf(stderr, "short read, read %d frames\n", rc);
        }

        /*pump sound out*/
        rc = snd_pcm_writei(play_handle, buffer, frames);
        if (rc == -EPIPE) {
            /* EPIPE means underrun */
            fprintf(stderr, "underrun occurred\n");
            snd_pcm_prepare(play_handle);
        } else if (rc < 0) {
            fprintf(stderr,
                "error from writei: %s\n",
                snd_strerror(rc));
        }  else if (rc != (int)frames) {
            fprintf(stderr,
                "short write, write %d frames\n", rc);
        }
    }
    
    snd_pcm_drain(capture_handle);
    snd_pcm_close(capture_handle);
    snd_pcm_close (capture_handle);

    snd_pcm_drain(play_handle);
    snd_pcm_close(play_handle);
    snd_pcm_close (play_handle);
    return 0;
}
