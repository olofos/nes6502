#ifndef NES_CPU_H
#define NES_CPU_H

#include <stdint.h>

#define FLAG_CARRY     0x01
#define FLAG_ZERO      0x02
#define FLAG_INTERRUPT 0x04
#define FLAG_DECIMAL   0x08
#define FLAG_BREAK     0x10
#define FLAG_CONSTANT  0x20
#define FLAG_OVERFLOW  0x40
#define FLAG_NEGATIVE  0x80

#define FLAG_NMI_PENDING 0x01
#define FLAG_IRQ_PENDING 0x04

struct nes_cpu
{
    uint16_t pc;
    uint8_t sp;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint8_t status;
    uint8_t irq_pending;
};

extern struct nes_cpu cpu;

uint8_t read_mem(uint16_t address);
void write_mem(uint16_t address, uint8_t val);

int cpu_step(void);

#endif
