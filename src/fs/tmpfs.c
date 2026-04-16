#include <fs/tmpfs.h>
#include <lib/kmalloc.h>
#include <lib/kstring.h>

#define TMPFS_MAX_CHILDREN 64

typedef struct tmpfs_node_data {
    uint8_t    *buf;
    uint64_t    buf_size;
    vfs_node_t *children[TMPFS_MAX_CHILDREN];
    int         child_count;
} tmpfs_data_t;


static vfs_node_t *tmpfs_alloc_node(const char *name, uint32_t flags);
static vfs_ops_t   tmpfs_file_ops;
static vfs_ops_t   tmpfs_dir_ops;


static int64_t tmpfs_read(vfs_node_t *node, void *buf, uint64_t offset, uint64_t size)
{
    tmpfs_data_t *d = (tmpfs_data_t *)node->fs_data;
    if (!d->buf || offset >= d->buf_size) return 0;
    uint64_t avail = d->buf_size - offset;
    uint64_t to_read = size < avail ? size : avail;
    kmemcpy(buf, d->buf + offset, to_read);
    return (int64_t)to_read;
}

static int64_t tmpfs_write(vfs_node_t *node, const void *buf, uint64_t offset, uint64_t size)
{
    tmpfs_data_t *d = (tmpfs_data_t *)node->fs_data;
    uint64_t needed = offset + size;

    if (needed > d->buf_size) {
        uint8_t *new_buf = kmalloc(needed);
        if (!new_buf) return -1;
        for (uint64_t i = 0; i < needed; i++) new_buf[i] = 0;
        if (d->buf) {
            kmemcpy(new_buf, d->buf, d->buf_size);
            kfree(d->buf);
        }
        d->buf      = new_buf;
        d->buf_size = needed;
        node->size  = needed;
    }

    kmemcpy(d->buf + offset, buf, size);
    return (int64_t)size;
}

static vfs_node_t *tmpfs_finddir(vfs_node_t *node, const char *name)
{
    tmpfs_data_t *d = (tmpfs_data_t *)node->fs_data;
    for (int i = 0; i < d->child_count; i++) {
        if (kstrcmp(d->children[i]->name, name) == 0)
            return d->children[i];
    }
    return NULL;
}

static vfs_node_t *tmpfs_readdir(vfs_node_t *node, uint32_t index)
{
    tmpfs_data_t *d = (tmpfs_data_t *)node->fs_data;
    if ((int)index >= d->child_count) return NULL;
    return d->children[index];
}

static vfs_node_t *tmpfs_mkdir(vfs_node_t *parent, const char *name)
{
    tmpfs_data_t *pd = (tmpfs_data_t *)parent->fs_data;
    if (pd->child_count >= TMPFS_MAX_CHILDREN) return NULL;

    vfs_node_t *node = tmpfs_alloc_node(name, VFS_FLAG_DIR);
    if (!node) return NULL;

    node->parent = parent;
    pd->children[pd->child_count++] = node;
    return node;
}

static vfs_node_t *tmpfs_mkfile(vfs_node_t *parent, const char *name)
{
    tmpfs_data_t *pd = (tmpfs_data_t *)parent->fs_data;
    if (pd->child_count >= TMPFS_MAX_CHILDREN) return NULL;

    vfs_node_t *node = tmpfs_alloc_node(name, VFS_FLAG_FILE);
    if (!node) return NULL;

    node->parent = parent;
    pd->children[pd->child_count++] = node;
    return node;
}

static int tmpfs_unlink(vfs_node_t *parent, const char *name)
{
    tmpfs_data_t *pd = (tmpfs_data_t *)parent->fs_data;
    for (int i = 0; i < pd->child_count; i++) {
        if (kstrcmp(pd->children[i]->name, name) == 0) {
            for (int j = i; j < pd->child_count - 1; j++)
                pd->children[j] = pd->children[j + 1];
            pd->child_count--;
            return 0;
        }
    }
    return -1;
}

static vfs_ops_t tmpfs_file_ops = {
    .read    = tmpfs_read,
    .write   = tmpfs_write,
    .finddir = NULL,
    .readdir = NULL,
    .mkdir   = NULL,
    .mkfile  = NULL,
    .unlink  = NULL,
};

static vfs_ops_t tmpfs_dir_ops = {
    .read    = NULL,
    .write   = NULL,
    .finddir = tmpfs_finddir,
    .readdir = tmpfs_readdir,
    .mkdir   = tmpfs_mkdir,
    .mkfile  = tmpfs_mkfile,
    .unlink  = tmpfs_unlink,
};

static vfs_node_t *tmpfs_alloc_node(const char *name, uint32_t flags)
{
    vfs_node_t   *node = kzalloc(sizeof(vfs_node_t));
    tmpfs_data_t *data = kzalloc(sizeof(tmpfs_data_t));
    if (!node || !data) return NULL;

    kstrcpy(node->name, name, VFS_MAX_NAME);
    node->flags   = flags;
    node->fs_data = data;
    node->ops     = (flags & VFS_FLAG_DIR) ? &tmpfs_dir_ops : &tmpfs_file_ops;
    return node;
}

vfs_node_t *tmpfs_create(void)
{
    return tmpfs_alloc_node("/", VFS_FLAG_DIR);
}
