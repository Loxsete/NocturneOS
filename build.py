#!/usr/bin/env python3
"""build for lxtos"""

import os
import subprocess
import sys
import glob

CC = "gcc"
LD = "ld"
AS = "nasm"

CFLAGS = [
    "-m64", "-ffreestanding", "-fno-stack-protector",
    "-fno-pic", "-fno-pie", "-mno-red-zone",
    "-mno-mmx", "-mno-sse", "-mno-sse2",
    "-mcmodel=kernel", "-nostdlib",
    "-Wall", "-Wextra", "-O2",
    "-Iinclude"
]
ASFLAGS = ["-f", "elf64"]
LDFLAGS = [
    "-m", "elf_x86_64", "-nostdlib", "-static",
    "-T", "linker.ld", "-z", "max-page-size=0x1000"
]

KERNEL = "kernel.elf"
ISO = "lxtos.iso"
DISK_IMG = "disk.img"

LIMINE_REPO = "https://codeberg.org/Limine/Limine.git"
LIMINE_DIR = "limine"


def run(cmd, **kwargs):
    print(" ".join(cmd))
    result = subprocess.run(cmd, **kwargs)
    if result.returncode != 0:
        print(f"Error: command finished with code {result.returncode}")
        sys.exit(result.returncode)


def sh(cmd):
    print(cmd)
    ret = os.system(cmd)
    if ret != 0:
        print(f"Error: '{cmd}' finished with code {ret}")
        sys.exit(1)


def newer(src, dst):
    if not os.path.exists(dst):
        return True
    return os.path.getmtime(src) > os.path.getmtime(dst)


def find_files(directory, extension):
    return glob.glob(f"{directory}/**/*{extension}", recursive=True)


def get_sources():
    jobs = []
    for src in find_files("src", ".c"):
        rel = os.path.relpath(src, "src")
        obj = os.path.join("build", rel[:-2] + ".o")
        jobs.append(("c", src, obj))
    for src in find_files("userspace/src", ".c"):
        rel = os.path.relpath(src, "userspace/src")
        obj = os.path.join("build/user", rel[:-2] + ".o")
        jobs.append(("user_c", src, obj))
    for src in find_files("src", ".asm"):
        rel = os.path.relpath(src, "src")
        obj = os.path.join("build", rel[:-4] + "_asm.o")
        jobs.append(("asm", src, obj))
    return jobs


def build_initramfs():
    os.makedirs("build", exist_ok=True)
    os.makedirs("initramfs/bin", exist_ok=True)
    os.makedirs("initramfs/etc", exist_ok=True)
    with open("initramfs/etc/osname", "w") as f:
        f.write("lxtos\n")
    sh("cd initramfs && find . | cpio -o -H newc > ../build/initramfs.cpio")
    run([LD, "-r", "-b", "binary",
         "build/initramfs.cpio", "-o", "build/initramfs_bin.o"])


def compile_sources():
    jobs = get_sources()
    obj_files = []
    for kind, src, obj in jobs:
        obj_files.append(obj)
        os.makedirs(os.path.dirname(obj), exist_ok=True)
        if not newer(src, obj):
            continue
        if kind == "c":
            run([CC] + CFLAGS + ["-c", src, "-o", obj])
        elif kind == "user_c":
            run([CC] + CFLAGS + ["-Iuserspace/include", "-c", src, "-o", obj])
        elif kind == "asm":
            run([AS] + ASFLAGS + [src, "-o", obj])
    return obj_files


def build_kernel():
    build_initramfs()
    obj_files = compile_sources()
    obj_files.append("build/initramfs_bin.o")
    run([LD] + LDFLAGS + ["-o", KERNEL] + obj_files)
    if os.path.exists(LIMINE_DIR):
        sh(f"rm -rf {LIMINE_DIR}")


def clone_limine():
    if not os.path.exists(LIMINE_DIR):
        run(["git", "clone", "--branch=v11.x-binary", "--depth=1",
             LIMINE_REPO, LIMINE_DIR])


def build_iso():
    build_kernel()
    clone_limine()
    os.makedirs("iso_root/boot/limine", exist_ok=True)
    os.makedirs("iso_root/EFI/BOOT", exist_ok=True)
    sh(f"cp {KERNEL} iso_root/boot/")
    sh("cp limine.conf iso_root/boot/limine/")
    for f in ["limine-bios.sys", "limine-bios-cd.bin", "limine-uefi-cd.bin"]:
        sh(f"cp {LIMINE_DIR}/{f} iso_root/boot/limine/")
    sh(f"cp {LIMINE_DIR}/BOOTX64.EFI iso_root/EFI/BOOT/")
    sh(f"cp {LIMINE_DIR}/BOOTIA32.EFI iso_root/EFI/BOOT/")
    run([
        "xorriso", "-as", "mkisofs",
        "-b", "boot/limine/limine-bios-cd.bin",
        "-no-emul-boot", "-boot-load-size", "4", "-boot-info-table",
        "--efi-boot", "boot/limine/limine-uefi-cd.bin",
        "-efi-boot-part", "--efi-boot-image",
        "--protective-msdos-label",
        "iso_root", "-o", ISO
    ])


def create_disk():
    if not os.path.exists(DISK_IMG):
        run(["dd", "if=/dev/zero", f"of={DISK_IMG}", "bs=1M", "count=64"])


def run_qemu():
    build_iso()
    create_disk()
    run([
        "qemu-system-x86_64",
        "-cdrom", ISO,
        "-drive", f"file={DISK_IMG},format=raw,if=ide,index=0",
        "-m", "128M",
        "-vga", "std",
        "-no-reboot",
        "-no-shutdown"
    ])


def clean():
    sh("rm -rf build iso_root limine")
    sh(f"rm -f {KERNEL} {ISO}")


TARGETS = {
    "all":   build_kernel,
    "iso":   build_iso,
    "run":   run_qemu,
    "clean": clean,
}

if __name__ == "__main__":
    target = sys.argv[1] if len(sys.argv) > 1 else "all"
    if target not in TARGETS:
        print(f"Unknown target: '{target}'")
        print(f"Available: {', '.join(TARGETS)}")
        sys.exit(1)
    TARGETS[target]()
