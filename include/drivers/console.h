#pragma once
#include <fs/vfs.h>

vfs_node_t *console_create(void);
void console_init_node(vfs_node_t *node);