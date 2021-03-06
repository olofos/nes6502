#include "nes_apu.h"
#include "nes_cpu.h"
#include "config.h"

#include <math.h>
#include <stdio.h>

channel_t *current_channel;

void apu_reset(void)
{
    for(int i = 0; i < 3; i++) {
        apu.channels[i].conf = i;
        apu.channels[i].phase = 0;
        apu.channels[i].muted = 1;
        apu.channels[i].period = 1;
    }

    apu.volume = 255;

    for(uint16_t a = 0x4000; a <= 0x4013; a++) {
        write_apu(a, (a & 3) ? 0x00 : 0x01);
    }

    write_apu(0x4015, 0x0F);
    write_apu(0x4017, 0x40);
}

void apu_run(uint32_t cpu_cycles)
{
    apu.cycles += cpu_cycles;

    if(apu.mode & FLAG_APU_FIVE_STEP_SEQUENCE) {
        while(apu.cycles > 7456) {
            apu.cycles -= 7457;

            current_channel = &apu.channels[CHAN_SQ1];
            frame_update_sq();

            current_channel = &apu.channels[CHAN_SQ2];
            frame_update_sq();

            current_channel = &apu.channels[CHAN_TRI];
            frame_update_tri();

            current_channel = &apu.channels[CHAN_NOISE];
            frame_update_noise();

            apu.step++;
            if(apu.step > 4) {
                apu.step = 0;
            }
        }
    } else {
        while(apu.cycles > 7456) {
            apu.cycles -= 7457;

            current_channel = &apu.channels[CHAN_SQ1];
            frame_update_sq();

            current_channel = &apu.channels[CHAN_SQ2];
            frame_update_sq();

            current_channel = &apu.channels[CHAN_TRI];
            frame_update_tri();

            current_channel = &apu.channels[CHAN_NOISE];
            frame_update_noise();

            apu.step++;
            if(apu.step > 3) {
                apu.step = 0;
                if(!(apu.mode & FLAG_APU_IRQ_INHIBIT)) {
                    cpu.irq_pending |= FLAG_IRQ_PENDING;
                }
            }
        }
    }
}


static uint8_t apu_backup[32];

uint8_t read_apu(uint16_t address)
{
    switch(address)
    {
    case 0x4015:
    {
        uint8_t old_irq = cpu.irq_pending & FLAG_IRQ_PENDING;
        cpu.irq_pending &= ~FLAG_IRQ_PENDING;
        if(!old_irq) {
            return 0xFF;
        } else {
            return 0x7F;
        }
    }

    case 0x4017:
        return apu.mode;
    }

    return apu_backup[address-0x4000];
}

void write_apu(uint16_t address, uint8_t val)
{
    apu_backup[address-0x4000] = val;
    switch(address)
    {
    case 0x4015:
        break;

    case 0x4017:
        apu.cycles = 0;
        apu.step = 0;
        apu.mode = val;

        if(val & FLAG_APU_IRQ_INHIBIT) {
            cpu.irq_pending &= ~FLAG_IRQ_PENDING;
        }
        break;
    }

    current_channel = &apu.channels[CHAN_SQ1];
    write_reg_sq1(address & 0xFF, val);

    current_channel = &apu.channels[CHAN_SQ2];
    write_reg_sq2(address & 0xFF, val);

    current_channel = &apu.channels[CHAN_TRI];
    write_reg_tri(address & 0xFF, val);

    current_channel = &apu.channels[CHAN_NOISE];
    write_reg_noise(address & 0xFF, val);

    // if(((address >= 0x4004) && (address <= 0x4007)) || (address == 0x4015) || (address == 0x4017)) {
    //     printf("%04X: %02X p2: %ld v: %d l: %d p: %d ph: %f r:%d\n", address, val, apu.channels[1].period, apu.channels[1].volume, apu.channels[1].length_counter, apu.channels[1].period, apu.channels[1].phase, apu.channels[1].timer_running);
    // }

    // if(((address >= 0x4000) && (address <= 0x4003)) || (address == 0x4015) || (address == 0x4017)) {
    //     printf("%04X: %02X p1: %d v: %d l: %d ph: %f m:%d s: %d\n", address, val, apu.channels[0].period, apu.channels[0].volume, apu.channels[0].length_counter, apu.channels[0].phase, apu.channels[0].muted);
    // }

    // if(((address >= 0x4008) && (address <= 0x400B)) || (address == 0x4015) || (address == 0x4017)) {
    //     printf("%04X: %02X t: %d l: %d ll: %d lr: %d lc: %d lh: %d p: %d ph: %f m:%d\n", address, val, apu.channels[2].period, apu.channels[2].length_counter, apu.channels[2].linear_counter, apu.channels[2].linear_counter_reload_value, apu.channels[2].linear_counter_reload_flag, apu.channels[2].length_counter_halt_flag, apu.channels[2].period, apu.channels[2].phase, apu.channels[2].muted);
    // }
}

void channel_timer_start(void)
{
    current_channel->muted = 0;
}

void channel_timer_stop(void)
{
    current_channel->muted = 1;
}

void channel_timer_set_period(uint16_t period)
{
    current_channel->reload_period = current_channel->period;
}


static uint8_t next_sample_sq(double freq, int k)
{
    channel_t *ch = &apu.channels[k];
    if(!ch->muted) {
        ch->phase += F_CPU / freq;
        if(ch->phase > ch->reload_period) {
            ch->phase -= ch->reload_period;
            ch->step = (ch->step + 1) & 0x0F;
        }
    }

    if(ch->step < ch->duty_cycle) {
        return ch->volume;
    } else {
        return 0;
    }
}

static uint8_t next_sample_tri(double freq)
{
    channel_t *ch = &apu.channels[CHAN_TRI];

    if(!ch->muted) {
        ch->phase += F_CPU / freq;
        if(ch->phase > ch->reload_period) {
            ch->phase -= ch->reload_period;
            ch->step = (ch->step + 1) & 0x1F;
        }
    }

    return wave_buf[ch->step];
}

static uint8_t next_sample_noise(double freq)
{
    channel_t *ch = &apu.channels[CHAN_NOISE];

    if(!ch->muted) {
        ch->phase += F_CPU / freq;
        if(ch->phase > ch->reload_period) {
            ch->phase -= ch->reload_period;

            uint16_t feedback;

            if(ch->shift_mode & _BV(SHIFT_MODE_BIT))
            {
                feedback = ((ch->shift_register & _BV(0)) ^ ((ch->shift_register & _BV(6)) >> 6));
            } else {
                feedback = ((ch->shift_register & _BV(0)) ^ ((ch->shift_register & _BV(1)) >> 1));
            }

            ch->shift_register >>= 1;

            if(feedback)
            {
                ch->shift_register |= 0x4000;
            }
        }
    }

    if(!ch->shift_register) {
        ch->shift_register = 0x0001;
    }

    if(ch->shift_register & _BV(0)) {
        return ch->volume;
    } else {
        return 0;
    }
}

const char *note_string(char *s, int n)
{
    static const char* note_names[] = {" C", "C#", " D", "D#", " E", " F", "F#", " G", "G#", " A", "A#", " B"};
    sprintf(s, "%s%-2d", note_names[(n-3-1 + 12) % 12], (n-3+12) / 12);
    return s;
}

int16_t apu_next_sample(double freq)
{
    const double p1 = (apu.mute & _BV(CHAN_SQ1)) ? 0 : next_sample_sq(freq, CHAN_SQ1);
    const double p2 = (apu.mute & _BV(CHAN_SQ2)) ? 0 : next_sample_sq(freq, CHAN_SQ2);
    const double t = (apu.mute & _BV(CHAN_TRI)) ? 0 : next_sample_tri(freq);
    const double n = (apu.mute & _BV(CHAN_NOISE)) ? 0 : next_sample_noise(freq);
    const double d = 64;

    const double out_pulse = 95.88 / (100.0 + 8128.0 / (p1 + p2));
    const double out_tri_noise_dmc = 159.79 / (100.0 + 1.0 / ( (t / 8227.0) + (n / 12241.0) + (d / 22638)));
    const double out = out_pulse + out_tri_noise_dmc - 0.5;

    double freq_p1 = 1789773.0/(16.0*(apu.channels[0].period + 1));
    double freq_p2 = 1789773.0/(16.0*(apu.channels[1].period + 1));
    double freq_t = 1789773.0/(2*16.0*(apu.channels[2].period + 1));

    int num_p1 = 49 + 12 * log2(freq_p1 / 442.0);
    int num_p2 = 49 + 12 * log2(freq_p2 / 442.0);
    int num_t = 49 + 12 * log2(freq_t / 442.0);

    char s1[32], s2[32], s3[32];
    printf("\r[%s] [%s] [%s] [%s]",
           ((!apu.channels[0].muted) && (apu.channels[0].volume > 0)) ? note_string(s1, num_p1) : "    ",
           ((!apu.channels[1].muted) && (apu.channels[1].volume > 0)) ? note_string(s2, num_p2) : "    ",
           (!apu.channels[2].muted) ? note_string(s3, num_t) : "    ",
           ((!apu.channels[3].muted) && (apu.channels[3].volume > 0)) ? "X" : " "
        );

    return (int16_t) ((apu.volume / 255.0) * INT16_MAX * out);
}
