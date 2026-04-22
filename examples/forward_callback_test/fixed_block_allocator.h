/*
 * MIT License
 *
 * Copyright (c) 2026 LONGZR007
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef FIXED_BLOCK_ALLOCATOR_H
#define FIXED_BLOCK_ALLOCATOR_H

#include <stddef.h>

// 内存操作锁宏定义
// 多线程环境下，用户应该定义这些宏来提供线程安全的锁机制
// 例如：
// #define FIXED_BLOCK_LOCK()   pthread_mutex_lock(&g_allocator_lock)
// #define FIXED_BLOCK_UNLOCK() pthread_mutex_unlock(&g_allocator_lock)
#ifndef FIXED_BLOCK_LOCK
#define FIXED_BLOCK_LOCK()   /* 默认空实现，多线程环境下必须定义 */
#endif

#ifndef FIXED_BLOCK_UNLOCK
#define FIXED_BLOCK_UNLOCK() /* 默认空实现，多线程环境下必须定义 */
#endif

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
