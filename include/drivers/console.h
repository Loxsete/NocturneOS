#pragma once
#include <fs/vfs.h>


// Create a new console device
vfs_node_t *console_create(void);

// Initialize the console device
void console_init_node(vfs_node_t *node);