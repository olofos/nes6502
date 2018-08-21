#ifndef NES_CPU_H
#define NES_CPU_H

#include <stdint.h>

struct nes_cpu
{
    uint16_t pc;
    uint8_t sp;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t status;
};

extern struct nes_cpu cpu;

uint8_t read_mem(uint16_t address);
uint8_t log_read_mem(uint16_t address);
void write_mem(uint16_t address, uint8_t val);

int cpu_step(void);

#endif
