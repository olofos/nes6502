#include "nes_apu.h"
#include "nes_cpu.h"
#include "config.h"


void apu_reset(void)
{
    apu.channels[CHAN_SQ1].conf = CHAN_SQ1;
    apu.channels[CHAN_SQ2].conf = CHAN_SQ2;
    apu.channels[CHAN_TRI].conf = CHAN_TRI;
    apu.channels[CHAN_NOISE].conf = CHAN_NOISE;

    for(uint16_t a = 0x4000; a <= 0x4013; a++) {
        write_apu(a, 0x00);
    }

    write_apu(0x4015, 0x0F);
    write_apu(0x4017, 0x40);
}

void apu_run(uint32_t cpu_cycles)
{
    apu.cycles += cpu_cycles;

    if(apu.mode & FLAG_APU_FIVE_STEP_SEQUENCE) {
        while(apu.cycles > 7456) {
            apu.cycles -= 7456;

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
            apu.cycles -= 7456;

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
                    // printf("IRQ\n");
                    // cpu.irq_pending |= FLAG_IRQ_PENDING;
                }
            }
        }
    }
}


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

    return 0xFF;
}

channel_t *current_channel;

void write_apu(uint16_t address, uint8_t val)
{
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
}

void channel_timer_start(void)
{
    current_channel->timer_running = 1;
}

void channel_timer_stop(void)
{
    if(current_channel->timer_running) {
        current_channel->timer_running = 0;
    }
}

void channel_timer_set_period(uint16_t period)
{
    current_channel->reload_period = current_channel->period;
}


static uint8_t next_sample_sq(double freq, int k)
{
    channel_t *ch = &apu.channels[k];
    if(ch->timer_running && (ch->length_counter > 0)) {
        ch->phase += F_CPU / freq;
        if(ch->phase > ch->reload_period) {
            ch->phase -= ch->reload_period;
            ch->step = (ch->step + 1) & 0x0F;
        }
    }

    if(ch->step < ch->duty_cycle && ch->enabled) {
        return ch->volume;
    } else {
        return 0;
    }
}

static uint8_t next_sample_tri(double freq)
{
    channel_t *ch = &apu.channels[CHAN_TRI];

    if(ch->timer_running && (ch->linear_counter > 0) && (ch->length_counter > 0) && ch->enabled) {
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

    if(ch->timer_running && ch->enabled && (ch->length_counter > 0)) {
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

int16_t apu_next_sample(double freq)
{
    const double p1 = next_sample_sq(freq, CHAN_SQ1);
    const double p2 = next_sample_sq(freq, CHAN_SQ2);
    const double t = next_sample_tri(freq);
    const double n = next_sample_noise(freq);
    const double d = 64;

    const double out_pulse = 95.88 / (100.0 + 8128.0 / (p1 + p2));
    const double out_tri_noise_dmc = 159.79 / (100.0 + 1.0 / ( (t / 8227.0) + (n / 12241.0) + (d / 22638)));
    const double out = out_pulse + out_tri_noise_dmc - 0.5;

    return (int16_t) (INT16_MAX * out);
}
