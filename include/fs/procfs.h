#pragma once
#include <fs/vfs.h>

typedef int64_t (*procfs_read_fn)(void *buf, uint64_t size);

// Create the /proc filesystem
vfs_node_t *procfs_create(void);

// Register a new /proc entry
vfs_node_t *procfs_register(const char *name, procfs_read_fn fn);

// Read from the /proc/mem entry
int64_t proc_mem_read(void *buf, uint64_t max);

// Read from the /proc/cpuinfo entry
int64_t proc_cpu_read(void *buf, uint64_t max);
