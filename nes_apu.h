#ifndef NES_APU_H_
#define NES_APU_H_

#include <stdint.h>

#include "nes_apu_channel.h"

#define FLAG_APU_IRQ_INHIBIT 0x40
#define FLAG_APU_FIVE_STEP_SEQUENCE 0x80

struct nes_apu
{
    uint8_t mode;
    uint16_t cycles;
    uint8_t step;

    uint8_t volume;
    uint8_t mute;

    channel_t channels[4];
};

extern struct nes_apu apu;

void apu_run(uint32_t cpu_cycles);
void apu_reset(void);
int16_t apu_next_sample(double freq);

uint8_t read_apu(uint16_t address);
void write_apu(uint16_t address, uint8_t val);


#endif
