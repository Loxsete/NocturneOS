#pragma once
#include <stdint.h>
#include <stddef.h>

#define VFS_FLAG_FILE      0x01
#define VFS_FLAG_DIR       0x02
#define VFS_FLAG_CHARDEV   0x04
#define VFS_FLAG_BLOCKDEV  0x08
#define VFS_FLAG_SYMLINK   0x10
#define VFS_FLAG_MOUNTPT   0x20

#define VFS_MAX_NAME  256
#define VFS_MAX_PATH  1024
#define VFS_MAX_MOUNTS 16
#define VFS_MAX_FDS    64

struct vfs_node;

typedef struct vfs_ops {
    int              (*open)   (struct vfs_node *node, int flags);
    int              (*close)  (struct vfs_node *node);
    int64_t          (*read)   (struct vfs_node *node, void *buf, uint64_t offset, uint64_t size);
    int64_t          (*write)  (struct vfs_node *node, const void *buf, uint64_t offset, uint64_t size);
    struct vfs_node *(*finddir)(struct vfs_node *node, const char *name);
    struct vfs_node *(*readdir)(struct vfs_node *node, uint32_t index);
    struct vfs_node *(*mkdir)  (struct vfs_node *node, const char *name);
    struct vfs_node *(*mkfile) (struct vfs_node *node, const char *name);
    int              (*unlink) (struct vfs_node *node, const char *name);
} vfs_ops_t;

typedef struct vfs_node {
    char             name[VFS_MAX_NAME];
    uint32_t         flags;
    uint64_t         size;
    void            *fs_data;
    vfs_ops_t       *ops;
    struct vfs_node *parent;
    struct vfs_node *mount;
} vfs_node_t;

typedef struct {
    char        path[VFS_MAX_PATH];
    vfs_node_t *root;
} vfs_mount_t;


extern vfs_node_t *vfs_root;


// Virtual file system initialization
void        vfs_init(void);

// Mount a filesystem
int         vfs_mount(const char *path, vfs_node_t *fs_root);

// Resolve a path to a vfs_node
vfs_node_t *vfs_resolve(const char *path);

// Read from a vfs_node
int64_t     vfs_read (const char *path, void *buf, uint64_t offset, uint64_t size);

// Write to a vfs_node
int64_t     vfs_write(const char *path, const void *buf, uint64_t offset, uint64_t size);

// Create a directory
vfs_node_t *vfs_mkdir (const char *path);

// Create a file
vfs_node_t *vfs_mkfile(const char *path);

// Unlink a file or directory
int         vfs_unlink(const char *path);

// Virtual file system read operation
int64_t     vnode_read (vfs_node_t *node, void *buf, uint64_t offset, uint64_t size);

// Virtual file system write operation
int64_t     vnode_write(vfs_node_t *node, const void *buf, uint64_t offset, uint64_t size);

// Virtual file system find directory operation
vfs_node_t *vnode_finddir(vfs_node_t *node, const char *name);

// Virtual file system read directory operation
vfs_node_t *vnode_readdir(vfs_node_t *node, uint32_t index);

// Virtual file system mount operation
vfs_node_t *ext2_mount(int bus, int drive);
