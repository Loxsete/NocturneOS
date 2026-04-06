CC = gcc
LD = ld
CFLAGS = -m64 -ffreestanding -fno-stack-protector -fno-pic -fno-pie \
         -mno-red-zone -mno-mmx -mno-sse -mno-sse2 \
         -mcmodel=kernel \
         -nostdlib \
         -Wall -Wextra -O2 \
         -Iinclude
LDFLAGS = -m elf_x86_64 -nostdlib -static -T linker.ld -z max-page-size=0x1000
KERNEL = kernel.elf
ISO = lxtos.iso
DISK_IMG = disk.img
LIMINE_REPO = https://codeberg.org/Limine/Limine.git
LIMINE_DIR = limine
SRCS = $(shell find src -name '*.c')
OBJS = $(patsubst src/%.c, build/%.o, $(SRCS))

.PHONY: all clean run iso initramfs limine disk

all: initramfs $(KERNEL)
	@rm -rf limine

$(KERNEL): $(OBJS) build/initramfs_bin.o
	$(LD) $(LDFLAGS) -o $@ $^

build/%.o: src/%.c
	@mkdir -p build
	$(CC) $(CFLAGS) -c $< -o $@

initramfs:
	@mkdir -p build initramfs/bin initramfs/etc
	echo "lxtos" > initramfs/etc/osname
	cd initramfs && find . | cpio -o -H newc > ../build/initramfs.cpio
	$(LD) -r -b binary build/initramfs.cpio -o build/initramfs_bin.o

disk:
	@if [ ! -f $(DISK_IMG) ]; then \
		dd if=/dev/zero of=$(DISK_IMG) bs=1M count=64; \
	fi

$(LIMINE_DIR):
	git clone --branch=v11.x-binary --depth=1 $(LIMINE_REPO) $(LIMINE_DIR)

limine: $(LIMINE_DIR)

iso: all limine
	mkdir -p iso_root/boot/limine
	cp $(KERNEL) iso_root/boot/
	cp limine.conf iso_root/boot/limine/
	cp $(LIMINE_DIR)/limine-bios.sys iso_root/boot/limine/
	cp $(LIMINE_DIR)/limine-bios-cd.bin iso_root/boot/limine/
	cp $(LIMINE_DIR)/limine-uefi-cd.bin iso_root/boot/limine/
	mkdir -p iso_root/EFI/BOOT
	cp $(LIMINE_DIR)/BOOTX64.EFI iso_root/EFI/BOOT/
	cp $(LIMINE_DIR)/BOOTIA32.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs \
		-b boot/limine/limine-bios-cd.bin \
		-no-emul-boot -boot-load-size 4 -boot-info-table \
		--efi-boot boot/limine/limine-uefi-cd.bin \
		-efi-boot-part --efi-boot-image \
		--protective-msdos-label \
		iso_root -o $(ISO)

run: iso disk
	qemu-system-x86_64 \
		-cdrom $(ISO) \
		-drive file=$(DISK_IMG),format=raw,if=ide,index=0 \
		-m 128M \
		-vga std \
		-no-reboot \
		-no-shutdown

clean:
	rm -rf build
	rm -rf $(LIMINE_DIR)
	rm -rf iso_root
	rm -f $(KERNEL) $(ISO)