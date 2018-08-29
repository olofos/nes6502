#include <alsa/asoundlib.h>
#include <stdint.h>
#include <stdio.h>

#include "alsa_sound.h"

static snd_pcm_t *pcm_handle;
int16_t out_buf[OUT_SIZE];

void open_hardware(const char *device)
{
    int pcm_error;

    /* Open the PCM device in playback mode */
    pcm_error = snd_pcm_open(&pcm_handle, device, SND_PCM_STREAM_PLAYBACK, 0);
    if(pcm_error < 0) {
        printf("ERROR: Can't open \"%s\" PCM device. %s\n", device, snd_strerror(pcm_error));
        exit(1);
    }
}

void init_hardware(unsigned freq)
{
    int pcm_error;
    snd_pcm_hw_params_t *params;

    /* Allocate parameters object and fill it with default values*/

    snd_pcm_hw_params_alloca(&params);

    snd_pcm_hw_params_any(pcm_handle, params);

    /* Set parameters */
    pcm_error = snd_pcm_hw_params_set_access(pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    if(pcm_error < 0) {
        printf("ERROR: Can't set interleaved mode. %s\n", snd_strerror(pcm_error));
        exit(1);
    }

    pcm_error = snd_pcm_hw_params_set_format(pcm_handle, params, SND_PCM_FORMAT_S16_LE);
    if(pcm_error < 0) {
        printf("ERROR: Can't set format. %s\n", snd_strerror(pcm_error));
        exit(1);
    }

    pcm_error = snd_pcm_hw_params_set_channels(pcm_handle, params, 1);
    if(pcm_error < 0) {
        printf("ERROR: Can't set channels number. %s\n", snd_strerror(pcm_error));
        exit(1);
    }

    pcm_error = snd_pcm_hw_params_set_rate_near(pcm_handle, params, &freq, 0);
    if(pcm_error < 0) {
        printf("ERROR: Can't set rate. %s\n", snd_strerror(pcm_error));
        exit(1);
    }

    pcm_error = snd_pcm_hw_params_set_buffer_size(pcm_handle, params, OUT_SIZE);
    if(pcm_error < 0) {
        printf("ERROR: Can't set buffer size. %s\n", snd_strerror(pcm_error));
        exit(1);
    }


    /* Write parameters */
    pcm_error = snd_pcm_hw_params(pcm_handle, params);
    if(pcm_error < 0) {
        printf("ERROR: Can't set harware parameters. %s\n", snd_strerror(pcm_error));
        exit(1);
    }

    /* Resume information */
    printf("PCM name: '%s'\n", snd_pcm_name(pcm_handle));
    printf("PCM state: %s\n", snd_pcm_state_name(snd_pcm_state(pcm_handle)));

    snd_pcm_uframes_t f;
    snd_pcm_hw_params_get_buffer_size(params, &f);
    printf("PCM buffer size: %lu\n", f);

    unsigned channels;

    snd_pcm_hw_params_get_channels(params, &channels);
    printf("Channels: %i ", channels);

    if(channels == 1) {
        printf("(mono)\n");
    } else if (channels == 2) {
        printf("(stereo)\n");
    }

    unsigned rate;

    snd_pcm_hw_params_get_rate(params, &rate, 0);
    printf("Rate: %d bps\n", rate);
}

/* close what we've opened */
void close_hardware(void)
{
    snd_pcm_drop(pcm_handle);
    snd_pcm_close(pcm_handle);
}

void output_samples_alsa(const int16_t* buf, size_t count )
{
    int pcm_error = snd_pcm_writei(pcm_handle, buf, count);
    if (pcm_error == -EPIPE) {
        printf("XRUN.\n");
        snd_pcm_prepare(pcm_handle);
    } else if (pcm_error < 0) {
        printf("ERROR. Can't write to PCM device. %s\n", snd_strerror(pcm_error));
    }
}

void output_sample_alsa(int16_t sample)
{
    static int current_sample = 0;

    out_buf[current_sample++] = sample;
    if(current_sample >= OUT_SIZE) {
        output_samples_alsa(out_buf, OUT_SIZE);
        current_sample=0;
    }

}
