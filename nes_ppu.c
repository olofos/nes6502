#include "nes_ppu.h"
#include "nes_cpu.h"

#include <stdio.h>

void ppu_run(uint32_t cpu_cycles)
{
    ppu.cycles += 3 * cpu_cycles;
    if(ppu.cycles >= 341) {
        ppu.scanline++;
        ppu.cycles -= 341;
    }

    if(ppu.scanline == 0) {
        ppu.nmi |= FLAG_PPU_VBL;
    }

    if(ppu.scanline == 20) {
        ppu.status &= ~FLAG_PPU_VBL;
    }

    if(ppu.scanline > 261) {
        ppu.scanline -= 262;
    }

    uint8_t new_nmi = (ppu.control & FLAG_PPU_NMI) && (ppu.status & FLAG_PPU_VBL);

    if(new_nmi && !ppu.nmi) {
        printf("\n\nNMI\n\n");
        cpu.irq_pending |= FLAG_NMI_PENDING;
    }

    ppu.nmi = new_nmi;
};

uint8_t read_ppu(uint16_t address)
{
    switch(address)
    {
    case 0x2002:
    {
        uint8_t status = ppu.status;
        ppu.status &= ~FLAG_PPU_VBL;
        return status;
    }
    }

    return 0x00;
}

void write_ppu(uint16_t address, uint8_t val)
{
    switch(address)
    {
    case 0x2000:
        ppu.control = val;
        break;
    }
}
