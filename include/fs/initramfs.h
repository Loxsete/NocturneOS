#pragma once
#include <fs/vfs.h>
#include <stdint.h>


// Unpack the initramfs image
int initramfs_unpack(void *data, uint64_t size, vfs_node_t *root);
