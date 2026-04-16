#pragma once
#include <stdint.h>


typedef struct kernel_api {

    void     (*puts)   (const char *s);
    void     (*putchar)(char c);
    void     (*puthex) (uint64_t val);
    void     (*putdec) (uint64_t val);


    char     (*getchar)(void);


    void    *(*malloc) (uint64_t size);
    void     (*free)   (void *ptr);


    int      (*strlen) (const char *s);
    int      (*strcmp) (const char *a, const char *b);
    void     (*strcpy) (char *dst, const char *src, int max);
    void     (*memcpy) (void *dst, const void *src, uint64_t n);
    void     (*memset) (void *dst, uint8_t val, uint64_t n);


    int64_t  (*vfs_read)  (const char *path, void *buf, uint64_t offset, uint64_t size);
    int64_t  (*vfs_write) (const char *path, const void *buf, uint64_t offset, uint64_t size);
    void    *(*vfs_open)  (const char *path);
    int64_t  (*vfs_node_read) (void *node, void *buf, uint64_t offset, uint64_t size);


    uint32_t version;
} kernel_api_t;


typedef void (*kapp_entry_t)(kernel_api_t *api);
