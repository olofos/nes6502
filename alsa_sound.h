#ifndef ALSA_SOUND_H_
#define ALSA_SOUND_H_

#define OUT_SIZE 4096

void open_hardware(const char *device);
void init_hardware(unsigned freq);
void close_hardware(void);
void output_samples_alsa(const int16_t* buf, size_t count);
void output_sample_alsa(int16_t sample);

#endif
