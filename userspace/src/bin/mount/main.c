#include <libc/stdio.h>
#include <libc/stdlib.h>
#include <ulib/syscall.h>

int main(int argc, char **argv)
{
    // usage:
    // mount <source> <mountpoint> [fstype]

    if (argc < 3) {
        printf("usage: %s <source> <mountpoint> [fstype]\n", argv[0]);
        printf("example: %s disk0 /mnt ext2\n", argv[0]);
        exit(1);
    }

    const char *source = argv[1];
    const char *mountpoint = argv[2];
    const char *fstype = (argc >= 4) ? argv[3] : "auto";

    int64_t res = sys_mount(source, mountpoint, fstype);

    if (res == 0) {
        puts("mounted\n");
    } else {
        puts("mount failed\n");
    }

    return 0;
}
