#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

#include "nes_cpu.h"
#include "nes_apu_channel.h"
#include "nes_apu.h"
#include "nsf.h"
#include "alsa_sound.h"
#include "config.h"

#define DATFILE

#ifdef DATFILE
#include "dat_file.h"
DatFileHandle dat;
#endif


#define OUT_FREQ 44100UL

#define BANK_SIZE 4096
#define ROM_BASE 0x8000

struct nes_apu apu;

uint8_t uses_bankswitching = 0;
uint8_t num_banks = 0;


uint8_t low_ram[2048];
uint8_t sram[2*4096];
uint8_t rom_data[1024 * 16];

#define MAX_BANKS 8
#define MAX_LOADED_BANKS 256

uint8_t rom_banks[MAX_LOADED_BANKS][BANK_SIZE];
int16_t loaded_banks[MAX_LOADED_BANKS];
uint8_t bank_map[MAX_BANKS];

uint8_t apu_written;

uint8_t read_mem(uint16_t address)
{
    if(address < 0x800) {
        return low_ram[address];
    } else if(((address >= 0x4000) && (address <= 0x4013)) || (address == 0x4015) || (address == 0x4017)) {
        return read_apu(address);
    } else if((address >=  0x6000) && (address < 0x8000)) {
        return sram[address - 0x6000];
    } else if(address >= ROM_BASE) {
        if(uses_bankswitching) {
            int b = (address - ROM_BASE) / BANK_SIZE;
            return rom_banks[bank_map[b]][address & (BANK_SIZE - 1)];
        } else {
            return rom_data[address - ROM_BASE];
        }
    }else {
        // printf("Read from 0x%04X\n", address);
        return 0x00;
    }
}

void write_mem(uint16_t address, uint8_t val)
{
    if(address < 0x800) {
        low_ram[address] = val;
    } else if(((address >= 0x4000) && (address <= 0x4013)) || (address == 0x4015) || (address == 0x4017)) {
#ifdef DATFILE
        dat_reg_write(dat, address & 0x00FF, val);
#endif
        write_apu(address, val);
        if(address <= 0x4013) {
            apu_written = 1;
        }
    } else if((address >= 0x5FF8) & (address <= 0x5FFF)) {
        if(uses_bankswitching) {
            int b = address - 0x5FF8;
            // if(bank_map[b] != val) {
            //     printf("Bank switch: %d -> %d\n", b, val);
            // }


            dat_reg_write(dat, 0x0b + b, val);

            bank_map[b] = val;
        }
    } else if((address >=  0x6000) && (address < 0x8000)) {
        sram[address - 0x6000] = val;
    } else {
        // printf("Write to 0x%04X = 0x%02X\n", address, val);
    }
}


void play_song(int track, struct nsf_header header)
{
    printf("Playing track %d\n", track);

    if(uses_bankswitching) {
        for(int i = 0; i < MAX_BANKS; i++) {
            bank_map[i] = (header.banks[i] < num_banks) ? header.banks[i] : 0;
        }
    }

#ifdef DATFILE
    dat = dat_new();
    dat_new_frame(dat);
#endif

    apu_reset();

    memset(low_ram, 0, sizeof(low_ram));
    memset(sram, 0, sizeof(sram));

    write_mem(0x100 + 0xFF, 0xFF);
    write_mem(0x100 + 0xFE, 0xFE);

    write_mem(0x4017, 0x00);
    write_mem(0x4015, 0x00);

    for(uint16_t a = 0x4000; a <= 0x4013; a++) {
        write_mem(a, (a & 3) ? 0x00 : 0x10);
    }


    write_mem(0x4000, 0x00);
    write_mem(0x4015, 0x0F);
    write_mem(0x4017, 0x00);


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

    int16_t prev_sample;
    uint32_t silence_counter = 0;

    uint32_t frame_counter = 0;

#ifdef PCLOG
    FILE *inst_log = fopen("inst.log", "w");
#endif

    const uint32_t play_time = 120;
    const uint32_t fade_time = 3;
    const double frames_per_second = ((double) 1000000 / header.ntsc_speed);

    uint16_t silent_frames = 0;

    while((silence_counter < 3*OUT_FREQ) && (frame_counter++ < frames_per_second * play_time)) {

        apu_written = 0;

        if(frame_counter > frames_per_second * (play_time - fade_time)) {
            apu.volume = 255 * (frames_per_second * play_time - frame_counter) / ((double) frames_per_second * fade_time);
        }

        write_mem(0x100 + 0xFF, 0xFF);
        write_mem(0x100 + 0xFE, 0xFE);

        cpu.pc = header.play;
        cpu.sp = 0xFF - 2;

        cpu.clock = 0;

        while(cpu_step() == 0) {
            apu_run(cpu.clock - prev_clock);
            prev_clock = cpu.clock;

#ifdef PCLOG
            fprintf(inst_log, "%04X\n", cpu.pc);
#endif

            if(cpu.pc == 0xFFFF) {

                break;
            }
        }

#ifdef PCLOG
        fprintf(inst_log, "\n");
#endif

        apu.mode |= FLAG_APU_IRQ_INHIBIT;

        apu_run((F_CPU / ((double) 1000000 / header.ntsc_speed)) - cpu.clock);

#ifdef DATFILE
        dat_new_frame(dat);
#endif


        if(apu_written) {
            silent_frames = 0;
        } else {
            silent_frames++;
        }

        for(int s = 0; s < OUT_FREQ / ((double) 1000000 / header.ntsc_speed); s++) {
            int16_t sample = apu_next_sample(OUT_FREQ);
            output_sample_alsa(sample);

            if(sample == prev_sample) {
                silence_counter++;
            } else {
                silence_counter = 0;
                prev_sample = sample;
            }
        }
    }

    printf("\n%d frame with no APU writes\n", silent_frames);

    printf("\n");

#ifdef PCLOG
    fclose(inst_log);
#endif

#ifdef DATFILE
    dat_save_binary(dat, "out.bin");
    dat_save_ascii(dat, "out.dat");
#endif

}

int main(int argc, char *argv[])
{
    // const char *filename = "/home/oohls/Documents/Projects/music/NSF/The Legend Of Zelda (NTSC) (PRG1) (SFX).nsf";
    //const char *filename = "/home/oohls/Documents/Projects/music/NSF/Metroid (NTSC (SFX).nsf";
    // const char *filename = "/home/oohls/Dropbox/Projects/music-files/Metroid.nsf";
    const char *filename = "/home/oohls/Documents/Projects/music/NSF/Maniac Mansion (US) (1990 - Realtime Associates) (SFX) Rip #1.nsf";

    int play_single_track = 0;
    int track = 1;

    if(argc > 1) {
        filename = argv[1];
    }

    if(argc > 2) {
        track = strtol(argv[2], 0, 10);
        play_single_track = 1;
    }

    FILE *f = fopen(filename, "rb");

    if(!f) {
        fprintf(stderr, "Could not open \"%s\"\n", filename);
        return 1;
    }

    fseek(f, 0, SEEK_END);
    int filesize = ftell(f);
    fseek(f, 0, SEEK_SET);

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

    for(int i = 0; i < MAX_LOADED_BANKS; i++) {
        loaded_banks[i] = -1;
    }

    for(int i = 0; i < MAX_BANKS; i++) {
        if(header.banks[i] != 0) {
            uses_bankswitching = 1;
        }
    }

    printf("File size: %d bytes\n", filesize);

    if(uses_bankswitching) {
        printf("Uses bank switching. First bank loaded at 0x%04x (offset %d)\n", ROM_BASE + (header.load & 0xFFF), (header.load & 0xFFF));
        printf("Total number of banks in file: %lu\n", 1 + (filesize + (header.load & 0xFFF) - sizeof(struct nsf_header)) / BANK_SIZE);

        for(uint16_t offset = header.load & 0xFFF, n = BANK_SIZE; n == BANK_SIZE; offset = 0, num_banks++) {
            printf("Loading bank %2d. ", num_banks);
            n = fread(&rom_banks[num_banks][offset], 1, BANK_SIZE-offset, f) + offset;
            printf("Read %d bytes.\n", n);
        }
    } else {
        fread(&rom_data[header.load - ROM_BASE], 1, filesize - sizeof(struct nsf_header), f);
    }

    open_hardware("default");
    init_hardware(OUT_FREQ);

    if(play_single_track) {
        play_song(track, header);
    } else {
        for(int i = 0; i < header.num_songs; i++) {
            play_song(i+1, header);
        }
    }

    close_hardware();
}
