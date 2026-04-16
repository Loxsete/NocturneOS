#pragma once
#include <stdint.h>

typedef enum { ATA_MASTER = 0, ATA_SLAVE = 1 } ata_drive_t;

typedef struct {
    int      present;
    char     model[41];
    uint32_t sectors;
} ata_disk_t;

extern ata_disk_t ata_disks[2][2];


// Initialize the ATA subsystem
void ata_init(void);

// Read a sector from the specified ATA drive
int  ata_read_sector(int bus, ata_drive_t drive, uint32_t lba, uint16_t *buf);

// Write a sector to the specified ATA drive
int ata_write_sector(int bus, ata_drive_t drive, uint32_t lba, uint16_t *buf);