#include "eproto_fixed_block_allocator.h"
#include <stdio.h>

#define X(size, count) static char eproto_mem_##size##_##count[count][size];
CONFIG_EPROTO_FIXED_BLOCK_POOLS
#undef X

static const size_t eproto_pool_sizes[] = {
#define X(size, count) size,
    CONFIG_EPROTO_FIXED_BLOCK_POOLS
#undef X
};

static const size_t eproto_pool_counts[] = {
#define X(size, count) count,
    CONFIG_EPROTO_FIXED_BLOCK_POOLS
#undef X
};

static char *eproto_pool_memories[] = {
#define X(size, count) &eproto_mem_##size##_##count[0][0],
    CONFIG_EPROTO_FIXED_BLOCK_POOLS
#undef X
};

static eproto_fixed_block_allocator_t g_eproto_fixed_block_allocator;
static int g_eproto_initialized = 0;

void eproto_fixed_block_allocator_init(void) {
    if (g_eproto_initialized) {
        return;
    }

    g_eproto_fixed_block_allocator.pool_count = EPROTO_POOL_COUNT;

    for (size_t i = 0; i < EPROTO_POOL_COUNT; i++) {
        eproto_fixed_block_pool_t *pool = &g_eproto_fixed_block_allocator.pools[i];
        pool->block_size = eproto_pool_sizes[i];
        pool->block_count = eproto_pool_counts[i];
        pool->used_count = 0;
        pool->max_used_count = 0;
        pool->free_list = NULL;
        pool->memory = eproto_pool_memories[i];

        for (size_t j = 0; j < pool->block_count; j++) {
            eproto_fixed_block_t *block = (eproto_fixed_block_t *)(pool->memory + j * pool->block_size);
            block->next = pool->free_list;
            block->pool_size = pool->block_size;
            pool->free_list = block;
        }
    }

    g_eproto_initialized = 1;
}

void *eproto_fixed_block_alloc(size_t size) {
    size_t pool_index = 0;
    while (pool_index < g_eproto_fixed_block_allocator.pool_count) {
        eproto_fixed_block_pool_t *pool = &g_eproto_fixed_block_allocator.pools[pool_index];
        if (pool->block_size >= size) {
            if (pool->free_list) {
                eproto_fixed_block_t *block = pool->free_list;
                pool->free_list = block->next;

                pool->used_count++;
                if (pool->used_count > pool->max_used_count) {
                    pool->max_used_count = pool->used_count;
                }

                return (void *)(block + 1);
            }
        }
        pool_index++;
    }

    return NULL;
}

void eproto_fixed_block_free(void *ptr) {
    if (!ptr) {
        return;
    }

    eproto_fixed_block_t *block = (eproto_fixed_block_t *)ptr - 1;
    size_t pool_size = block->pool_size;

    for (size_t i = 0; i < g_eproto_fixed_block_allocator.pool_count; i++) {
        eproto_fixed_block_pool_t *pool = &g_eproto_fixed_block_allocator.pools[i];
        if (pool->block_size == pool_size) {
            block->next = pool->free_list;
            pool->free_list = block;

            if (pool->used_count > 0) {
                pool->used_count--;
            }
            break;
        }
    }
}

void eproto_fixed_block_allocator_stats(void) {
    printf("Eproto Fixed Block Allocator Stats:\n");
    for (size_t i = 0; i < g_eproto_fixed_block_allocator.pool_count; i++) {
        eproto_fixed_block_pool_t *pool = &g_eproto_fixed_block_allocator.pools[i];
        printf("Pool size: %zu bytes, Total blocks: %zu, Used blocks: %zu, Max used: %zu\n", pool->block_size,
               pool->block_count, pool->used_count, pool->max_used_count);
    }
}
