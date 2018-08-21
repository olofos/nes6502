#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "nes_cpu.h"

#define FLAT_RAM 1

struct nsf_header
{
    uint8_t magic[5];
    uint8_t version;
    uint8_t num_songs;
    uint8_t start_song;
    uint16_t load;
    uint16_t init;
    uint16_t play;
    char name[32];
    char artist[32];
    char copyright[32];
    uint16_t ntsc_speed;
    uint8_t banks[8];
    uint16_t pal_speed;
    uint8_t pal_ntsc_flags;
    uint8_t chip_flags;
    uint8_t reserved[4];
};

struct ines_header
{
    uint8_t magic[4];
    uint8_t prg_rom_size;
    uint8_t chr_rom_size;
    uint8_t flags6;
    uint8_t flags7;
    uint8_t prg_ram_size;
    uint8_t flags9;
    uint8_t flags10;
    uint8_t reserved[5];
};

#define BANK_SIZE 4096
#define ROM_BASE 0x8000

uint8_t *rom_data;

#ifdef FLAT_RAM
uint8_t ram[64 * 0x0400];

uint8_t read_mem(uint16_t address)
{
    return ram[address];
}

uint8_t log_read_mem(uint16_t address)
{
    return ram[address];
}

void write_mem(uint16_t address, uint8_t val)
{
    ram[address] = val;
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

uint8_t log_read_mem(uint16_t address)
{
    if(address < 0x800) {
        return ram[address];
    } else if(address < 0x8000) {
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


    if(argc > 1) {
        filename = argv[1];
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

        int rom_size = BANK_SIZE * (1 + (filesize - sizeof(struct nsf_header) - 1) / BANK_SIZE);
        rom_data = malloc(rom_size);
        memset(rom_data, 0, rom_size);

        if(uses_bankswitching) {
            printf("Uses bank switching. First bank loaded at 0x%04x (offset %d)\n", ROM_BASE + (header.load & 0xFFF), (header.load & 0xFFF));
            printf("Total number of banks in file: %lu\n", 1 + (filesize + (header.load & 0xFFF) - sizeof(struct nsf_header)) / BANK_SIZE);
        } else {
            int n = fread(&rom_data[header.load - ROM_BASE], 1, filesize - sizeof(struct nsf_header), f);
            if(n != filesize - sizeof(struct nsf_header)) {
                printf("Couldn't read full file (%d/%lu)?\n", n, filesize - sizeof(struct nsf_header));
            }

            cpu.pc = header.init;
            cpu.a = 2;
            cpu.x = 0;
            cpu.sp = 0xFF;

            while(cpu_step() == 0) {
            }

            printf("\n");

            cpu.pc = header.play;
            // cpu.a = 0;
            // cpu.x = 0;
            cpu.sp = 0xFF;

            while(cpu_step() == 0) {
            }

            printf("\n");
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


        cpu.pc = 0xC000;
        cpu.a = 0;
        cpu.x = 0;
        cpu.sp = 0xFD;
        cpu.status = 0x24;

        while((cpu_step() == 0) && (cpu.sp != 0xFF)) {
        }
    }
}

