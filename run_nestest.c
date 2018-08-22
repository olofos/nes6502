#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "nes_cpu.h"
#include "ines.h"

uint8_t ram[64 * 0x0400];

uint8_t read_mem(uint16_t address)
{
    return ram[address];
}

void write_mem(uint16_t address, uint8_t val)
{
    ram[address] = val;
}


int main(int argc, char *argv[])
{
    const char *filename = "nestest.nes";

    if(argc > 1) {
        filename = argv[1];
    }

    FILE *f = fopen(filename, "rb");

    if(!f) {
        fprintf(stderr, "Could not open \"%s\"\n", filename);
        return 1;
    }

    struct ines_header header;

    fread(&header, sizeof(header), 1, f);

    if(memcmp(header.magic, "NES\x1a", sizeof(header.magic))) {
        fprintf(stderr, "Wrong iNes magic string\n");
        return 1;
    }

    fread(&ram[0x8000], 1, header.prg_rom_size * 0x4000, f);

    if(header.prg_rom_size == 1) {
        memcpy(&ram[0xC000], &ram[0x8000], 0x4000);
    }

    for(uint16_t a = 0x4000; a <= 0x4017; a++) {
        ram[a] = 0xFF;
    }

    ram[0xFFF8] = 0xBA;
    ram[0xFFF9] = 0xAD;

    ram[0x100 + 0xFF] = 0xFF;
    ram[0x100 + 0xFE] = 0xF8 - 1;

    cpu.pc = 0xC000;
    cpu.a = 0;
    cpu.x = 0;
    cpu.sp = 0xFD;
    cpu.status = 0x24;

    while((cpu_step() == 0) && (cpu.pc < 0xFFF8)) {
    }

    if((ram[2] != 0x00) || (ram[3] != 0x00)) {
        printf("Error: 0x%02X 0x%02X\n", ram[2], ram[3]);
    }
}
