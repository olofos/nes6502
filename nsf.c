#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "nes_cpu.h"
#include "nes_apu_channel.h"
#include "nes_apu.h"
#include "ines.h"
#include "nsf.h"
#include "alsa_sound.h"
#include "config.h"

/// ALSA /////////////////////////////////////////////////////////////////////////////////////

#define OUT_FREQ 44100UL

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

#define FLAT_RAM 1

#define BANK_SIZE 4096
#define ROM_BASE 0x8000

uint8_t *rom_data;

#ifdef FLAT_RAM
uint8_t ram[64 * 0x0400];

#define FLAG_PPU_NMI 0x80
#define FLAG_PPU_VBL 0x80

struct nes_apu apu;

struct nes_ppu
{
    uint8_t control;
    uint8_t status;
    uint8_t nmi;
    uint16_t cycles;
    uint16_t scanline;
} ppu;

void ppu_run(uint8_t cpu_cycles)
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

uint8_t read_ppu(uint16_t address);
void write_ppu(uint16_t address, uint8_t val);

uint8_t read_mem(uint16_t address)
{
    // if(((address >= 0x2000) && (address <= 0x2007)) || (address == 0x4014)) {
    //     return read_ppu(address);
    // } else
        if(((address >= 0x4000) && (address <= 0x4013)) || (address == 0x4015) || (address == 0x4017)) {
        return read_apu(address);
    } else {
        return ram[address];
    }
}

void write_mem(uint16_t address, uint8_t val)
{
    // if(((address >= 0x2000) && (address <= 0x2007)) || (address == 0x4014)) {
    //     write_ppu(address, val);
    // } else
        if(((address >= 0x4000) && (address <= 0x4013)) || (address == 0x4015) || (address == 0x4017)) {
        write_apu(address, val);
    } else {
        ram[address] = val;
    }
}


#else
uint8_t ram[2048];

uint8_t read_mem(uint16_t address)
{
    if(address < 0x800) {
        return ram[address];
    } else if(address < 0x8000) {
        printf("Read from 0x%04X\n", address);
        return 0xAC;
    } else {
        return rom_data[address - 0x8000];
    }
}

void write_mem(uint16_t address, uint8_t val)
{
    if(address < 0x800) {
        ram[address] = val;
    } else {
        printf("Write to 0x%04X = 0x%02X\n", address, val);
    }
}
#endif

int main(int argc, char *argv[])
{
    // const char *filename = "/home/oohls/Documents/Projects/music/NSF/The Legend Of Zelda (NTSC) (PRG1) (SFX).nsf";
    const char *filename = "/home/oohls/Documents/Projects/music/NSF/Metroid (NTSC (SFX).nsf";
    // const char *filename = "/home/oohls/Dropbox/Projects/music-files/Metroid.nsf";

    int track = 1;

    if(argc > 1) {
        filename = argv[1];
    }

    if(argc > 2) {
        track = strtol(argv[2], 0, 10);
    }

    FILE *f = fopen(filename, "rb");

    if(!f) {
        fprintf(stderr, "Could not open \"%s\"\n", filename);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    int filesize = ftell(f);
    fseek(f, 0, SEEK_SET);

    if(!strcmp(&filename[strlen(filename)-4], ".nsf")) {
        struct nsf_header header;

        fread(&header, sizeof(header), 1, f);

        if(memcmp(header.magic, "NESM\x1a", sizeof(header.magic))) {
            fprintf(stderr, "Wrong NSF magic string\n");
            return 1;
        }

        printf("Song name: \"%.32s\"\n", header.name);
        printf("Artist:    \"%.32s\"\n", header.artist);
        printf("Copyright: \"%.32s\"\n", header.copyright);
        printf("Number of songs: %d\n", header.num_songs);
        printf("First song: %d\n", header.start_song);
        printf("Banks: %d %d %d %d %d %d %d %d\n", header.banks[0], header.banks[1], header.banks[2], header.banks[3], header.banks[4], header.banks[5], header.banks[6], header.banks[7]);
        printf("Load address: 0x%04x\n", header.load);
        printf("Init address: 0x%04x\n", header.init);
        printf("Play address: 0x%04x\n", header.play);
        printf("PAL/NTSC flags: 0x%02x\n", header.pal_ntsc_flags);
        printf("Chip flags: 0x%02x\n", header.chip_flags);
        printf("NTSC speed: %d\n", header.ntsc_speed);
        printf("PAL speed: %d\n", header.pal_speed);

        uint8_t uses_bankswitching = 0;

        for(int i = 0; i < 8; i++) {
            if(header.banks[i] != 0) {
                uses_bankswitching = 1;
            }
        }

        printf("File size: %d bytes\n", filesize);

        // int rom_size = BANK_SIZE * (1 + (filesize - sizeof(struct nsf_header) - 1) / BANK_SIZE);
        // rom_data = malloc(rom_size);
        // memset(rom_data, 0, rom_size);

        if(uses_bankswitching) {
            printf("Uses bank switching. First bank loaded at 0x%04x (offset %d)\n", ROM_BASE + (header.load & 0xFFF), (header.load & 0xFFF));
            printf("Total number of banks in file: %lu\n", 1 + (filesize + (header.load & 0xFFF) - sizeof(struct nsf_header)) / BANK_SIZE);
        } else {
            //int n = fread(&rom_data[header.load - ROM_BASE], 1, filesize - sizeof(struct nsf_header), f);

            open_hardware("default");
            init_hardware(OUT_FREQ);

            int n = fread(&ram[header.load], 1, filesize - sizeof(struct nsf_header), f);
            if(n != filesize - sizeof(struct nsf_header)) {
                printf("Couldn't read full file (%d/%lu)?\n", n, filesize - sizeof(struct nsf_header));
            }

            apu_reset();

            ram[0x100 + 0xFF] = 0xFF;
            ram[0x100 + 0xFE] = 0xFF-1;

            cpu.pc = header.init;
            cpu.a = track-1;
            cpu.x = 0;
            cpu.sp = 0xFF - 2;
            cpu.irq_pending = 0;

            uint32_t prev_clock = 0;

            while((cpu_step() == 0) && (cpu.pc < 0xFFFF)) {
                apu_run(cpu.clock - prev_clock);
                prev_clock = cpu.clock;
            }

            printf("\n");

            for(;;) {
                ram[0x100 + 0xFF] = 0xFF;
                ram[0x100 + 0xFE] = 0xFF-1;

                cpu.pc = header.play;
                cpu.sp = 0xFF - 2;

                cpu.clock = 0;

                while(cpu_step() == 0) {
                    apu_run(cpu.clock - prev_clock);
                    // frame_clock += cpu.clock - prev_clock;
                    prev_clock = cpu.clock;

                    if(cpu.pc == 0xFFFF) {
                        break;
                    }
                }

                apu_run((F_CPU / ((double) 1000000 / header.ntsc_speed)) - cpu.clock);

                for(int s = 0; s < OUT_FREQ / ((double) 1000000 / header.ntsc_speed); s++) {
                    output_sample_alsa(apu_next_sample(OUT_FREQ));
                }
            }

            close_hardware();
        }
    } else if(!strcmp(&filename[strlen(filename)-4], ".nes")) {
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

        ram[0x2002] = 0x80;

        ram[0x100 + 0xFF] = 0xFF;
        ram[0x100 + 0xFE] = 0xF8 - 1;


        cpu.pc = ((uint16_t) ram[0xFFFD] << 8) | ram[0xFFEC];
        //cpu.pc = 0xC000;
        cpu.a = 1;
        cpu.x = 0;
        cpu.sp = 0xFD;
        cpu.status = 0x24;

        // while((cpu_step() == 0) && (cpu.pc < 0xFFF6)) {
        //     if(ram[2] != 0 || ram[3] !=0) {
        //         printf("Error: 0x%02X 0x%02X\n", ram[2], ram[3]);
        //     }
        // }
        // printf("Error: 0x%02X 0x%02X\n", ram[2], ram[3]);

        char message[1000] = "";
        char prev_message[1000] = "";

        uint8_t status = 0xFF;

        uint16_t prev_pc = cpu.pc;
        uint32_t prev_clock = 0;

        while(cpu_step() == 0) {
            //if(cpu.pc >= 0xFFF8) break;

            ppu_run(cpu.clock - prev_clock);
            apu_run(cpu.clock - prev_clock);
            prev_clock = cpu.clock;

            if((ram[0x6001] == 0xDE) && (ram[0x6002] == 0xB0) && (ram[0x6003] == 0x61)) {
                strcpy(message, (char*)&ram[0x6004]);

                for(int i = 0; message[i]; i++) {
                    if(message[i] == '\n') {
                        message[i] = 0;
                    }
                }
                if(strcmp(message, prev_message)) {
                    if(strstr(message, prev_message) != message) {
                        printf("\n");
                    }
                    printf("\r%s                                                      ", message);
                    strcpy(prev_message, message);
                }
                if(ram[0x6000] != status) {
                    status = ram[0x6000];
                    printf("\nStatus %02X\n", ram[0x6000]);
                }

            }
            if(prev_pc == cpu.pc) {
                printf("Infinite loop detected!\n");
                printf("PPU C:%02X S:%02X\n", ppu.control, ppu.status);
                printf("APU M:%02X\n", apu.mode);
                printf("CPU P:%02X CYC:%d\n", cpu.status, cpu.clock);
                printf("Status %02X\n", ram[0x6000]);
                printf("\"%s\"\n", &ram[0x6004]);
                break;
            }
            prev_pc = cpu.pc;

        }
    }
}
