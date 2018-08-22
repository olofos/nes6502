#ifndef NSF_H
#define NSF_H

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

#endif
