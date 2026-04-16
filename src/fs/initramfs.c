#include <fs/initramfs.h>
#include <fs/vfs.h>
#include <lib/kmalloc.h>

#define CPIO_MAGIC "070701"
#define CPIO_MAGIC_CRC "070702"

typedef struct {
    char magic[6];
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char nlink[8];
    char mtime[8];
    char filesize[8];
    char devmajor[8];
    char devminor[8];
    char rdevmajor[8];
    char rdevminor[8];
    char namesize[8];
    char check[8];
} cpio_header_t;

static int kstrlen(const char *s) { int n=0; while(s[n]) n++; return n; }
static int kstrcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b){a++;b++;} return *a-*b;
}
static int kstrncmp(const char *a, const char *b, int n) {
    for (int i=0;i<n;i++){if(a[i]!=b[i])return a[i]-b[i];if(!a[i])return 0;} return 0;
}
static void kmemcpy(void *d, const void *s, uint64_t n) {
    uint8_t *dd=d; const uint8_t *ss=s;
    for(uint64_t i=0;i<n;i++) dd[i]=ss[i];
}

static uint32_t hex2u32(const char *s, int len)
{
    uint32_t val = 0;
    for (int i = 0; i < len; i++) {
        char c = s[i];
        uint32_t d;
        if      (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else d = 0;
        val = (val << 4) | d;
    }
    return val;
}

static uint64_t align4(uint64_t x) { return (x + 3) & ~3ULL; }

static vfs_node_t *mkpath(vfs_node_t *root, const char *path)
{
    if (!path || path[0] == '\0' || kstrcmp(path, ".") == 0) return root;

    if (path[0] == '.' && path[1] == '/') path += 2;
    if (path[0] == '/') path++;
    if (path[0] == '\0') return root;

    char part[VFS_MAX_NAME];
    int i = 0;
    while (path[i] && path[i] != '/' && i < VFS_MAX_NAME - 1) {
        part[i] = path[i]; i++;
    }
    part[i] = '\0';

    const char *rest = (path[i] == '/') ? path + i + 1 : path + i;

    vfs_node_t *child = vnode_finddir(root, part);
    if (!child) {
        if (*rest == '\0') return root;
        if (!root->ops || !root->ops->mkdir) return NULL;
        child = root->ops->mkdir(root, part);
        if (!child) return NULL;
    }

    if (*rest == '\0') return child;
    return mkpath(child, rest);
}


static const char *basename(const char *path)
{
    if (path[0] == '.' && path[1] == '/') path += 2;
    const char *last = path;
    for (const char *p = path; *p; p++)
        if (*p == '/') last = p + 1;
    return last;
}

int initramfs_unpack(void *data, uint64_t size, vfs_node_t *root)
{
    uint8_t  *ptr = (uint8_t *)data;
    uint8_t  *end = ptr + size;

    while (ptr + sizeof(cpio_header_t) <= end) {
        cpio_header_t *hdr = (cpio_header_t *)ptr;

        if (kstrncmp(hdr->magic, CPIO_MAGIC, 6) != 0 &&
            kstrncmp(hdr->magic, CPIO_MAGIC_CRC, 6) != 0)
            break;

        uint32_t namesize = hex2u32(hdr->namesize, 8);
        uint32_t filesize = hex2u32(hdr->filesize, 8);
        uint32_t mode     = hex2u32(hdr->mode, 8);

        ptr += sizeof(cpio_header_t);

        char *name = (char *)ptr;
        ptr += align4(sizeof(cpio_header_t) + namesize) - sizeof(cpio_header_t);

        uint8_t *file_data = ptr;
        ptr += align4(filesize);

        if (ptr > end) break;

        if (kstrcmp(name, "TRAILER!!!") == 0) break;

        if (kstrcmp(name, ".") == 0 || namesize <= 1) continue;

        const char *clean = name;
        if (clean[0] == '.' && clean[1] == '/') clean += 2;
        if (clean[0] == '/') clean++;

        int is_dir = (mode & 0170000) == 0040000;

        if (is_dir) {
            mkpath(root, clean);
        } else {
            const char *bname = basename(clean);
            vfs_node_t *parent = root;

            int path_len = kstrlen(clean);
            int base_len = kstrlen(bname);
            if (path_len > base_len) {
                char dir_path[VFS_MAX_PATH];
                int dlen = path_len - base_len - 1;
                if (dlen > 0 && dlen < VFS_MAX_PATH) {
                    for (int j = 0; j < dlen; j++) dir_path[j] = clean[j];
                    dir_path[dlen] = '\0';
                    parent = mkpath(root, dir_path);
                    if (!parent) continue;
                }
            }

            if (!parent->ops || !parent->ops->mkfile) continue;
            vfs_node_t *node = parent->ops->mkfile(parent, bname);
            if (!node) continue;

            if (filesize > 0 && node->ops && node->ops->write) {
                node->ops->write(node, file_data, 0, filesize);
            }
        }
    }

    return 0;
}
