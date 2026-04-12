#include <fs/vfs.h>
#include <lib/kmalloc.h>
#include <lib/kstring.h>

vfs_node_t *vfs_root = NULL;

static vfs_mount_t mount_table[VFS_MAX_MOUNTS];
static int         mount_count = 0;

void vfs_init(void)
{
    for (int i = 0; i < VFS_MAX_MOUNTS; i++)
        mount_table[i].root = NULL;
    mount_count = 0;
}

int vfs_mount(const char *path, vfs_node_t *fs_root)
{
    if (mount_count >= VFS_MAX_MOUNTS) return -1;
    if (kstrcmp(path, "/") == 0)
        vfs_root = fs_root;
    kstrcpy(mount_table[mount_count].path, path, VFS_MAX_PATH);
    mount_table[mount_count].root = fs_root;
    mount_count++;
    if (kstrcmp(path, "/") != 0) {
        vfs_node_t *node = vfs_resolve(path);
        if (node) node->mount = fs_root;
    }
    return 0;
}

static int split_path(const char *path, char parts[][VFS_MAX_NAME], int max_parts)
{
    int count = 0, i = 0, len = kstrlen(path);
    while (i < len && count < max_parts) {
        while (i < len && path[i] == '/') i++;
        if (i >= len) break;
        int j = 0;
        while (i < len && path[i] != '/' && j < VFS_MAX_NAME - 1)
            parts[count][j++] = path[i++];
        parts[count][j] = '\0';
        if (j > 0) count++;
    }
    return count;
}

vfs_node_t *vfs_resolve(const char *path)
{
    if (!vfs_root || !path) return NULL;
    if (kstrcmp(path, "/") == 0) return vfs_root;
    char parts[32][VFS_MAX_NAME];
    int  count = split_path(path, parts, 32);
    if (count == 0) return vfs_root;
    vfs_node_t *cur = vfs_root;
    for (int i = 0; i < count; i++) {
        if (!(cur->flags & VFS_FLAG_DIR)) return NULL;
        if (!cur->ops || !cur->ops->finddir) return NULL;
        vfs_node_t *next = cur->ops->finddir(cur, parts[i]);
        if (!next) return NULL;
        cur = next;
        if (cur->mount) cur = cur->mount;
    }
    return cur;
}

int64_t vnode_read(vfs_node_t *node, void *buf, uint64_t offset, uint64_t size)
{
    if (!node || !node->ops || !node->ops->read) return -1;
    return node->ops->read(node, buf, offset, size);
}

int64_t vnode_write(vfs_node_t *node, const void *buf, uint64_t offset, uint64_t size)
{
    if (!node || !node->ops || !node->ops->write) return -1;
    return node->ops->write(node, buf, offset, size);
}

vfs_node_t *vnode_finddir(vfs_node_t *node, const char *name)
{
    if (!node || !node->ops || !node->ops->finddir) return NULL;
    return node->ops->finddir(node, name);
}

vfs_node_t *vnode_readdir(vfs_node_t *node, uint32_t index)
{
    if (!node || !node->ops || !node->ops->readdir) return NULL;
    return node->ops->readdir(node, index);
}

int64_t vfs_read(const char *path, void *buf, uint64_t offset, uint64_t size)
{
    vfs_node_t *node = vfs_resolve(path);
    if (!node) return -1;
    return vnode_read(node, buf, offset, size);
}

int64_t vfs_write(const char *path, const void *buf, uint64_t offset, uint64_t size)
{
    vfs_node_t *node = vfs_resolve(path);
    if (!node) return -1;
    return vnode_write(node, buf, offset, size);
}

static void split_parent(const char *path, char *parent, char *name)
{
    int len = kstrlen(path), slash = -1;
    for (int i = len - 1; i >= 0; i--)
        if (path[i] == '/') { slash = i; break; }
    if (slash <= 0) { parent[0] = '/'; parent[1] = '\0'; }
    else { kstrcpy(parent, path, slash + 1); parent[slash] = '\0'; }
    kstrcpy(name, path + slash + 1, VFS_MAX_NAME);
}

vfs_node_t *vfs_mkdir(const char *path)
{
    char parent[VFS_MAX_PATH], name[VFS_MAX_NAME];
    split_parent(path, parent, name);
    vfs_node_t *dir = vfs_resolve(parent);
    if (!dir || !dir->ops || !dir->ops->mkdir) return NULL;
    return dir->ops->mkdir(dir, name);
}

vfs_node_t *vfs_mkfile(const char *path)
{
    char parent[VFS_MAX_PATH], name[VFS_MAX_NAME];
    split_parent(path, parent, name);
    vfs_node_t *dir = vfs_resolve(parent);
    if (!dir || !dir->ops || !dir->ops->mkfile) return NULL;
    return dir->ops->mkfile(dir, name);
}

int vfs_unlink(const char *path)
{
    char parent[VFS_MAX_PATH], name[VFS_MAX_NAME];
    split_parent(path, parent, name);
    vfs_node_t *dir = vfs_resolve(parent);
    if (!dir || !dir->ops || !dir->ops->unlink) return -1;
    return dir->ops->unlink(dir, name);
}
