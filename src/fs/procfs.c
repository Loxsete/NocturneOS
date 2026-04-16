#include <fs/procfs.h>
#include <lib/kmalloc.h>
#include <lib/kstring.h>
#include <lib/kitoa.h>
#include <mm/pmm.h>

#define PROCFS_MAX_ENTRIES 32

extern uint8_t kernel_heap[];
extern uint64_t heap_size;
extern uint64_t get_total_ram(void);


typedef struct {
    procfs_read_fn fn;
} procfs_file_data_t;

typedef struct {
    vfs_node_t *entries[PROCFS_MAX_ENTRIES];
    int         count;
} procfs_dir_data_t;

static vfs_node_t      *proc_root = NULL;
static procfs_dir_data_t proc_dir_data;

static int64_t procfs_read(vfs_node_t *node, void *buf, uint64_t offset, uint64_t size)
{
    procfs_file_data_t *d = (procfs_file_data_t *)node->fs_data;
    if (!d->fn) return 0;

    uint8_t tmp[4096];
    int64_t total = d->fn(tmp, sizeof(tmp));
    if (total <= 0) return 0;
    if (offset >= (uint64_t)total) return 0;

    uint64_t avail   = (uint64_t)total - offset;
    uint64_t to_read = size < avail ? size : avail;
    uint8_t *src = tmp + offset;
    uint8_t *dst = (uint8_t *)buf;
    for (uint64_t i = 0; i < to_read; i++) dst[i] = src[i];
    return (int64_t)to_read;
}

static vfs_node_t *procfs_finddir(vfs_node_t *node, const char *name)
{
    (void)node;
    for (int i = 0; i < proc_dir_data.count; i++) {
        if (kstrcmp(proc_dir_data.entries[i]->name, name) == 0)
            return proc_dir_data.entries[i];
    }
    return NULL;
}

static vfs_node_t *procfs_readdir_fn(vfs_node_t *node, uint32_t index)
{
    (void)node;
    if ((int)index >= proc_dir_data.count) return NULL;
    return proc_dir_data.entries[index];
}

static vfs_ops_t procfs_dir_ops = {
    .finddir = procfs_finddir,
    .readdir = procfs_readdir_fn,
};

static vfs_ops_t procfs_file_ops = {
    .read = procfs_read,
};

vfs_node_t *procfs_create(void)
{
    proc_root = kzalloc(sizeof(vfs_node_t));
    if (!proc_root) return NULL;
    kstrcpy(proc_root->name, "proc", VFS_MAX_NAME);
    proc_root->flags   = VFS_FLAG_DIR | VFS_FLAG_MOUNTPT;
    proc_root->fs_data = &proc_dir_data;
    proc_root->ops     = &procfs_dir_ops;
    proc_dir_data.count = 0;
    return proc_root;
}

vfs_node_t *procfs_register(const char *name, procfs_read_fn fn)
{
    if (!proc_root) return NULL;
    if (proc_dir_data.count >= PROCFS_MAX_ENTRIES) return NULL;

    vfs_node_t         *node = kzalloc(sizeof(vfs_node_t));
    procfs_file_data_t *data = kzalloc(sizeof(procfs_file_data_t));
    if (!node || !data) return NULL;

    kstrcpy(node->name, name, VFS_MAX_NAME);
    node->flags   = VFS_FLAG_FILE;
    node->ops     = &procfs_file_ops;
    node->fs_data = data;
    node->parent  = proc_root;
    data->fn      = fn;

    proc_dir_data.entries[proc_dir_data.count++] = node;
    return node;
}


int64_t proc_mem_read(void *buf, uint64_t max)
{
    char *out = (char *)buf;
    char tmp[32];
    int pos = 0;
    (void)max;

	
    const char *h0 = "mem total:  ";
    for (int i = 0; h0[i]; i++) out[pos++] = h0[i];
    kitoa(pmm_total_bytes() / 1024 / 1024, tmp);
    for (int i = 0; tmp[i]; i++) out[pos++] = tmp[i];
    const char *mb = " MB\n";
    for (int i = 0; mb[i]; i++) out[pos++] = mb[i];

    const char *h1 = "heap total: ";
    for (int i = 0; h1[i]; i++) out[pos++] = h1[i];
    kitoa(kmalloc_heap_size(), tmp);
    for (int i = 0; tmp[i]; i++) out[pos++] = tmp[i];
    out[pos++] = '\n';

    const char *h2 = "heap free:  ";
    for (int i = 0; h2[i]; i++) out[pos++] = h2[i];
    kitoa(kmalloc_free_size(), tmp);
    for (int i = 0; tmp[i]; i++) out[pos++] = tmp[i];
    out[pos++] = '\n';

    return pos;
}
static void cpuid(uint32_t leaf, uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
        : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
        : "a"(leaf), "c"(0));
}

int64_t proc_cpu_read(void *buf, uint64_t max)
{
    char *out = (char *)buf;
    int pos = 0;
    (void)max;

    uint32_t eax, ebx, ecx, edx;
    cpuid(0, &eax, &ebx, &ecx, &edx);

    char vendor[13];
    for (int i = 0; i < 4; i++) vendor[i]     = (ebx >> (i * 8)) & 0xFF;
    for (int i = 0; i < 4; i++) vendor[i + 4] = (edx >> (i * 8)) & 0xFF;
    for (int i = 0; i < 4; i++) vendor[i + 8] = (ecx >> (i * 8)) & 0xFF;
    vendor[12] = '\0';

    const char *v = "vendor: ";
    for (int i = 0; v[i]; i++) out[pos++] = v[i];
    for (int i = 0; vendor[i]; i++) out[pos++] = vendor[i];
    out[pos++] = '\n';

    uint32_t brand[12];
    cpuid(0x80000002, &brand[0],  &brand[1],  &brand[2],  &brand[3]);
    cpuid(0x80000003, &brand[4],  &brand[5],  &brand[6],  &brand[7]);
    cpuid(0x80000004, &brand[8],  &brand[9],  &brand[10], &brand[11]);

    const char *m = "model: ";
    for (int i = 0; m[i]; i++) out[pos++] = m[i];
    char *brand_str = (char *)brand;
    for (int i = 0; i < 48 && brand_str[i]; i++) out[pos++] = brand_str[i];
    out[pos++] = '\n';

    return pos;
}
