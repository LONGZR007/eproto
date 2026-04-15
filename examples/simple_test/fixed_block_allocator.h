#ifndef FIXED_BLOCK_ALLOCATOR_H
#define FIXED_BLOCK_ALLOCATOR_H

#include <stddef.h>

// 内存块结构
typedef struct fixed_block {
    struct fixed_block *next;  // 指向下一个空闲内存块
    size_t pool_size;          // 所属内存池的大小
} fixed_block_t;

// 内存池结构
typedef struct fixed_block_pool {
    size_t block_size;         // 内存块大小
    size_t block_count;        // 内存块总数
    size_t used_count;         // 当前使用的内存块数
    size_t max_used_count;     // 最大同时使用的内存块数
    fixed_block_t *free_list;    // 空闲内存块链表
    char *memory;              // 内存池的内存空间
} fixed_block_pool_t;

// 固定块内存分配器配置
#define CONFIG_FIXED_BLOCK_POOLS \
    X(32, 10)                   \
    X(64, 100)                  \
    X(128, 50)                  \
    X(256, 90)

// 使用 X 宏定义枚举来获取内存池数量
enum {
#define X(size, count) POOL_ENUM_##size##_##count,
    CONFIG_FIXED_BLOCK_POOLS
#undef X
    POOL_COUNT
};

// 内存分配器结构
typedef struct fixed_block_allocator {
    size_t pool_count;         // 内存池数量
    fixed_block_pool_t pools[POOL_COUNT];  // 内存池数组
} fixed_block_allocator_t;

// 函数声明
void fixed_block_allocator_init(void);
void *fixed_block_alloc(size_t size);
void fixed_block_free(void *ptr);
void fixed_block_allocator_stats(void);

#endif
