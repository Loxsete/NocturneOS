#include <drivers/ata.h>
#include <arch/x86_64/io.h>

#define ATA_PRIMARY_BASE   0x1F0
#define ATA_SECONDARY_BASE 0x170
#define ATA_PRIMARY_CTRL   0x3F6
#define ATA_SECONDARY_CTRL 0x376

#define ATA_REG_DATA     0x00
#define ATA_REG_ERROR    0x01 // reads error
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SECCOUNT 0x02
#define ATA_REG_LBA0     0x03
#define ATA_REG_LBA1     0x04
#define ATA_REG_LBA2     0x05
#define ATA_REG_HDDEVSEL 0x06 // choise disk master or slave
#define ATA_REG_COMMAND  0x07
#define ATA_REG_STATUS   0x07 // BSY/DRQ/ERR

#define ATA_CMD_IDENTIFY 0xEC // command define
#define ATA_CMD_READ_PIO 0x20 // first sector read pio
#define ATA_CMD_WRITE_PIO 0x30 // write sector pio
#define ATA_CMD_CACHE_FLUSH 0xE7

#define ATA_SR_BSY 0x80 // disk busy
#define ATA_SR_DRQ 0x08
#define ATA_SR_ERR 0x01 // o my god error

ata_disk_t ata_disks[2][2];

static uint16_t bases[2] = {ATA_PRIMARY_BASE, ATA_SECONDARY_BASE}; // port primary/secondary

// wait when BSY = 0
static void ata_wait_busy(uint16_t base) {
    while (inb(base + ATA_REG_STATUS) & ATA_SR_BSY);
}

// wait when DRQ = 1, -1 = error
static int ata_wait_drq(uint16_t base) {
    uint8_t status;
    while (1) {
        status = inb(base + ATA_REG_STATUS); // read status
        if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ)) return 0; // done
        if (status & ATA_SR_ERR) return -1; // error
    }
}

void ata_init(void) {
    for (int bus = 0; bus < 2; bus++) {
        for (int drv = 0; drv < 2; drv++) { // master/slave
            ata_disk_t *d = &ata_disks[bus][drv];
            d->present = 0; // no disk by def

            outb(bases[bus] + ATA_REG_HDDEVSEL, 0xA0 | (drv << 4)); // choise disk master or slave
            io_wait();

            // send command IDENTIFY
            outb(bases[bus] + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
            io_wait();

            uint8_t status = inb(bases[bus] + ATA_REG_STATUS);
            if (status == 0) continue; // no disk :(

            if (ata_wait_drq(bases[bus]) < 0) continue; // error, disk doesnt responding

            uint16_t buf[256];
            for (int i = 0; i < 256; i++)
                buf[i] = inw(bases[bus] + ATA_REG_DATA);

            for (int i = 0; i < 20; i++) {
                d->model[i * 2]     = (char)(buf[27 + i] >> 8);
                d->model[i * 2 + 1] = (char)(buf[27 + i] & 0xFF);
            }
            d->model[40] = '\0';

            for (int i = 39; i >= 0 && d->model[i] == ' '; i--)
                d->model[i] = '\0';

            d->sectors = ((uint32_t)buf[61] << 16) | buf[60];
            d->present = 1; // disk found!
        }
    }
}

int ata_read_sector(int bus, ata_drive_t drive, uint32_t lba, uint16_t *buffer) {
    uint16_t base = bases[bus];

    outb(base + ATA_REG_HDDEVSEL, 0xE0 | (drive << 4) | ((lba >> 24) & 0x0F)); // choise disk master or slave
    io_wait();
    outb(base + ATA_REG_SECCOUNT, 1);
    outb(base + ATA_REG_LBA0, (uint8_t)(lba));
    outb(base + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outb(base + ATA_REG_LBA2, (uint8_t)(lba >> 16));
    outb(base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);

    ata_wait_busy(base);
    if (ata_wait_drq(base) < 0) return -1; // error

    for (int i = 0; i < 256; i++)
        buffer[i] = inw(base + ATA_REG_DATA);

    return 0;
}


// ctrl c; ctrl v; xD
int ata_write_sector(int bus, ata_drive_t driver, uint32_t lba, uint16_t *buffer) {
    uint16_t base = bases[bus];

    outb(base + ATA_REG_HDDEVSEL, 0xE0 | (driver << 4) | ((lba >> 24) & 0x0F));
    io_wait();
    outb(base + ATA_REG_SECCOUNT, 1);
    outb(base + ATA_REG_LBA0, (uint8_t)(lba));
    outb(base + ATA_REG_LBA1, (uint8_t)(lba >> 8));
    outb(base + ATA_REG_LBA2, (uint8_t)(lba >> 16));
    outb(base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);

    ata_wait_busy(base);
    if (ata_wait_drq(base) < 0) return -1; // error

    for (int i = 0; i < 256; i++)
        outw(base + ATA_REG_DATA, buffer[i]); // write 256 words

    outb(base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_busy(base);

    return 0; 

}
