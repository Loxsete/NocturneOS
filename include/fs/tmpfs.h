#pragma once
#include <fs/vfs.h>


// Create the tmpfs filesystem
vfs_node_t *tmpfs_create(void);
