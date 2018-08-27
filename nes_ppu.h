#ifndef NES_PPU_H_
#define NES_PPU_H_

#include <stdint.h>

#define FLAG_PPU_NMI 0x80
#define FLAG_PPU_VBL 0x80

struct nes_ppu
{
    uint8_t control;
    uint8_t status;
    uint8_t nmi;
    uint16_t cycles;
    uint16_t scanline;
};

extern struct nes_ppu ppu;

uint8_t read_ppu(uint16_t address);
void write_ppu(uint16_t address, uint8_t val);
void ppu_run(uint32_t cpu_cycles);

#endif
