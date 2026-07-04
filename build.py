#!/usr/bin/env python3
"""Build system for Silex."""

import glob
import os
import sys
import argparse
import subprocess

CC = "gcc"
LD = "ld"
AS = "nasm"

CFLAGS_LIST = [
    "-m64",
    "-ffreestanding",
    "-fno-stack-protector",
    "-fno-pic",
    "-fno-pie",
    "-mno-red-zone",
    "-mno-mmx",
    "-mno-sse",
    "-mno-sse2",
    "-nostdlib",
    "-Wall",
    "-Wextra",
    "-O2"
]
LDFLAGS_LIST = [
    "-m", "elf_x86_64",
    "-nostdlib",
    "-static"
]
ASFLAGS_LIST = [
    "-f", "elf64"
]

KERN_CFLAGS_LIST = CFLAGS_LIST + ["-Iinclude", "-mcmodel=kernel"]
KERN_LDFLAGS_LIST = LDFLAGS_LIST + ["-T", "linker.ld", "-z", "max-page-size=0x1000"]
KERN_ASFLAGS_LIST = ASFLAGS_LIST

USER_CFLAGS_LIST = CFLAGS_LIST + ["-Iuserspace/include", "-Iinclude"]
USER_LDFLAGS_LIST = LDFLAGS_LIST + ["-T", "userspace/link.ld"]
USER_ASFLAGS_LIST = ASFLAGS_LIST

KERN_CFLAGS = ' '.join(KERN_CFLAGS_LIST)
KERN_ASFLAGS = ' '.join(KERN_ASFLAGS_LIST)
KERN_LDFLAGS = ' '.join(KERN_LDFLAGS_LIST)

USER_CFLAGS = ' '.join(USER_CFLAGS_LIST)
USER_LDFLAGS = ' '.join(USER_LDFLAGS_LIST)
USER_ASFLAGS = ' '.join(USER_ASFLAGS_LIST)

HOST_NAME = "hostpc"
KERNEL = "kernel.elf"
DISK_IMG = "disk.img"
ISO_ROOT_DIR = "iso_root"

LIMINE_REPO = "https://codeberg.org/Limine/Limine.git"
LIMINE_REPO_ARG = "--branch=v11.x-binary --depth=1"
LIMINE_DIR = "limine"

USER_COMMON = [
    "userspace/src/crt0.asm",
    "userspace/src/ulib/io.c",
    "userspace/src/ulib/string.c",
    "userspace/src/libc/string.c",
    "userspace/src/libc/stdio.c",
    "userspace/src/libc/stdlib.c",
]

def get_git_version():
    try:
        return subprocess.check_output(
            ["git", "describe", "--tags", "--always"],
            text=True
        ).strip()
    except:
        return "dev"

def load_release_info(path="release.conf"):
    data = {
        "NAME": "Unknown",
        "VERSION": "0",
        "CODENAME": "unknown",
        "ID": "unknown",
        "PRETTY_NAME": "",
        "GIT_VERSION": get_git_version(),
    }
    if os.path.exists(path):
        with open(path) as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if "=" not in line:
                    continue
                key, value = line.split("=", 1)
                key = key.strip()
                value = value.strip()
                if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
                    value = value[1:-1]
                data[key] = value
    if not data.get("PRETTY_NAME"):
        data["PRETTY_NAME"] = f'{data["NAME"]} {data["VERSION"]} ({data["CODENAME"]})'
    git_ver = data["GIT_VERSION"]
    if git_ver != "dev" and not data["VERSION"].endswith(f"+{git_ver}"):
        data["VERSION"] = f"{data['VERSION']}+{git_ver}"
        data["PRETTY_NAME"] = f'{data["NAME"]} {data["VERSION"]} ({data["CODENAME"]})'
    return data

_release = load_release_info()
OS_NAME = _release["NAME"]
OS_VERSION = _release["VERSION"]
OS_CODENAME = _release["CODENAME"]
OS_ID = _release["ID"]
OS_PRETTY = _release["PRETTY_NAME"]

ISO = f"{OS_NAME}.iso"

def sh(cmd: str, no_exit=False, quiet=False):
    if not quiet:
        print(cmd)
    ret = os.system(f"sh -c \"{cmd}\" {"&> /dev/null" if quiet else ""}")
    if ret != 0:
        print(f"Error: '{cmd}' finished with code {ret}")
        return ret if no_exit else sys.exit(1)

def newer(src: str, dst: str):
    if not os.path.exists(dst):
        return True
    return os.path.getmtime(src) > os.path.getmtime(dst)

def find_files(directory: str, extension: str):
    return glob.glob(f"{directory}/**/*{extension}", recursive=True)

def clone_repo(src_url: str, src_arg: str, dst_dir: str):
    if not os.path.exists(dst_dir):
        sh(f"git clone {src_arg} {src_url} {dst_dir}")

def build_user_binary(out_path: str, sources: list[str]):
    os.makedirs("build/ubin", exist_ok=True)
    objs = []
    for src in sources:
        safe = src.replace("/", "_").replace(".", "_")
        obj = f"build/ubin/{safe}.o"
        objs.append(obj)
        if not newer(src, obj):
            continue
        if src.endswith(".c"):
            sh(f"{CC} {USER_CFLAGS} -c {src} -o {obj}")
        elif src.endswith(".asm"):
            sh(f"{AS} {USER_ASFLAGS} {src} -o {obj}")
    sh(f"{LD} {USER_LDFLAGS} -o {out_path} {' '.join(objs)}")

SBIN_APPS = {"init"}

def build_userspace():
    os.makedirs("build/initramfs/bin", exist_ok=True)
    os.makedirs("build/initramfs/sbin", exist_ok=True)
    bin_dir = "userspace/src/bin"
    if not os.path.exists(bin_dir):
        return
    for app in os.listdir(bin_dir):
        app_path = os.path.join(bin_dir, app)
        if not os.path.isdir(app_path):
            continue
        sources = USER_COMMON.copy()
        for ext in ("*.c", "*.asm"):
            sources += glob.glob(os.path.join(app_path, "**", ext), recursive=True)
        if not sources:
            continue
        target_dir = "sbin" if app in SBIN_APPS else "bin"
        out = f"build/initramfs/{target_dir}/{app}"
        build_user_binary(out, sources)

def build_initramfs():
    os.makedirs("build/initramfs/etc", exist_ok=True)
    with open("build/initramfs/etc/os-release", "w") as f:
        f.write(f"NAME={OS_NAME}\n")
        f.write(f"VERSION={OS_VERSION}\n")
        f.write(f"CODENAME={OS_CODENAME}\n")
        f.write(f"ID={OS_ID}\n")
        f.write(f"PRETTY_NAME=\"{OS_PRETTY}\"\n")
    with open("build/initramfs/etc/hostname", "w") as f:
        f.write(f"{HOST_NAME}\n")

def initramfs_to_binary():
    build_initramfs()
    build_userspace()
    sh("cd build/initramfs && find . | cpio -o -H newc > ../initramfs.cpio")
    sh(f"{LD} -r -b binary build/initramfs.cpio -o build/initramfs_bin.o")

def get_kernel_sources():
    jobs = []
    for src in find_files("src", ".c"):
        rel = os.path.relpath(src, "src")
        obj = os.path.join("build", rel[:-2] + ".o")
        jobs.append(("c", src, obj))
    for src in find_files("src", ".asm"):
        rel = os.path.relpath(src, "src")
        obj = os.path.join("build", rel[:-4] + "_asm.o")
        jobs.append(("asm", src, obj))
    return jobs

def build_kernel_binaries():
    jobs = get_kernel_sources()
    obj_files = []
    for kind, src, obj in jobs:
        obj_files.append(obj)
        os.makedirs(os.path.dirname(obj), exist_ok=True)
        if not newer(src, obj):
            continue
        if kind == "c":
            sh(f"{CC} {KERN_CFLAGS} -c {src} -o {obj}")
        elif kind == "asm":
            sh(f"{AS} {KERN_ASFLAGS} {src} -o {obj}")
    return obj_files

def build_kernel():
    initramfs_to_binary()
    obj_files = build_kernel_binaries()
    obj_files.append("build/initramfs_bin.o")
    sh(f"{LD} {KERN_LDFLAGS} -o {KERNEL} {' '.join(obj_files)}")

def build_limine():
    clone_repo(LIMINE_REPO, LIMINE_REPO_ARG, LIMINE_DIR)
    os.makedirs(f"{ISO_ROOT_DIR}/boot/limine", exist_ok=True)
    sh(f"cp limine.conf {ISO_ROOT_DIR}/boot/limine/")
    for f in ["limine-bios.sys", "limine-bios-cd.bin", "limine-uefi-cd.bin"]:
        sh(f"cp {LIMINE_DIR}/{f} {ISO_ROOT_DIR}/boot/limine/")
    os.makedirs(f"{ISO_ROOT_DIR}/EFI/BOOT", exist_ok=True)
    for f in ["BOOTX64.EFI", "BOOTIA32.EFI"]:
        sh(f"cp {LIMINE_DIR}/{f} {ISO_ROOT_DIR}/EFI/BOOT/")

def build_iso():
    if not os.path.exists(KERNEL):
        print(f"Kernel file '{KERNEL}' not found, building it first...")
        exit(1)
    os.makedirs(f"{ISO_ROOT_DIR}/boot", exist_ok=True)
    sh(f"cp {KERNEL} {ISO_ROOT_DIR}/boot/")
    build_limine()
    sh(f"xorriso -as mkisofs -b boot/limine/limine-bios-cd.bin\
        -no-emul-boot -boot-load-size 4 -boot-info-table\
        --efi-boot boot/limine/limine-uefi-cd.bin -efi-boot-part\
        --efi-boot-image --protective-msdos-label {ISO_ROOT_DIR} -o {ISO}")

def create_disk():
    if not os.path.exists(DISK_IMG):
        sh(f"dd if=/dev/zero of={DISK_IMG} bs=1M count=64")
        sh(f"sudo mkfs.ext2 -b 1024 {DISK_IMG}")

def run_qemu():
    build_iso()
    create_disk()
    sh(f"qemu-system-x86_64 -cdrom {ISO}\
        -drive file={DISK_IMG},format=raw,if=ide,index=1\
        -m 128M -vga std -no-reboot -no-shutdown -enable-kvm")

def populate_disk():
    if not os.path.exists(DISK_IMG):
        sh(f"dd if=/dev/zero of={DISK_IMG} bs=1M count=64")
        sh(f"sudo mkfs.ext2 -b 1024 {DISK_IMG}")
    sh(f"mkdir -p /tmp/{OS_NAME}_mnt")
    sh(f"sudo mount -o loop {DISK_IMG} /tmp/{OS_NAME}_mnt")
    sh(f"sudo mkdir -p /tmp/{OS_NAME}_mnt/etc")
    sh(f"echo 'hello from ext2' | sudo tee /tmp/{OS_NAME}_mnt/etc/hello.txt")
    sh(f"echo '{OS_NAME}' | sudo tee /tmp/{OS_NAME}_mnt/etc/hostname")
    sh(f"sudo umount /tmp/{OS_NAME}_mnt")

def clean_build():
    sh("rm -rf build")

def clean():
    clean_build()
    sh(f"rm -rf {ISO_ROOT_DIR} {LIMINE_DIR}")
    sh(f"rm -f {KERNEL} {ISO}")

def clean_disk():
    sh(f"rm -f {DISK_IMG}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        prog="build.py",
        description=f"{OS_NAME} build script"
    )
    parser.add_argument("-b", "--build", choices=["all", "a", "kernel", "k", "iso", "i", "rebuild", "r"],
                        nargs="?", const="all", metavar="METHOD",
                        help="METHOD in [all/a, kernel/k, iso/i, rebuild/r], default: all")
    parser.add_argument("-r", "--run", action="store_true",
                        help="run a vm with OS img")
    parser.add_argument("-c", "--clean", choices=["all", "a", "disk", "d", "build", "b"],
                        nargs="?", const="all", metavar="METHOD",
                        help="METHOD in [all/a, disk/d, build/b], default: all")
    parser.add_argument("-p", "--populate-disk", action="store_true",
                        help="populate disk image with default files")
    args = parser.parse_args()
    kwargs = dict(args._get_kwargs())
    for arg, val in kwargs.items():
        if val is None or val is False:
            continue
        if arg == "build":
            if val in ["all", "a"]:
                build_kernel()
                build_iso()
            elif val in ["iso", "i"]:
                build_iso()
            elif val in ["kernel", "k"]:
                build_kernel()
            elif val in ["rebuild", "r"]:
                clean_build()
                build_kernel()
                build_iso()
        elif arg == "run":
            run_qemu()
        elif arg == "clean":
            if val in ["all", "a"]:
                clean()
            elif val in ["disk", "d"]:
                clean_disk()
            elif val in ["build", "b"]:
                clean_build()
        elif arg == "populate_disk":
            populate_disk()
