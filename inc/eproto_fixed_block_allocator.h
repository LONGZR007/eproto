#ifndef EPROTO_FIXED_BLOCK_ALLOCATOR_H
#define EPROTO_FIXED_BLOCK_ALLOCATOR_H

#include <stddef.h>
#include "eproto_def.h"

typedef struct eproto_fixed_block {
    struct eproto_fixed_block *next;
    size_t pool_size;
} eproto_fixed_block_t;

typedef struct eproto_fixed_block_pool {
    size_t block_size;
    size_t block_count;
    size_t used_count;
    size_t max_used_count;
    eproto_fixed_block_t *free_list;
    char *memory;
} eproto_fixed_block_pool_t;

enum {
#define X(size, count) EPROTO_POOL_ENUM_##size##_##count,
    CONFIG_EPROTO_FIXED_BLOCK_POOLS
#undef X
    EPROTO_POOL_COUNT
};

typedef struct eproto_fixed_block_allocator {
    size_t pool_count;
    eproto_fixed_block_pool_t pools[EPROTO_POOL_COUNT];
} eproto_fixed_block_allocator_t;

void eproto_fixed_block_allocator_init(void);
void *eproto_fixed_block_alloc(size_t size);
void eproto_fixed_block_free(void *ptr);
void eproto_fixed_block_allocator_stats(void);

#endif
