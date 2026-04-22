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

#include "fixed_block_allocator.h"
#include <stdio.h>

// 使用 X 宏定义静态内存数组
#define X(size, count) static char mem_##size##_##count[count][size];
CONFIG_FIXED_BLOCK_POOLS
#undef X

// 使用 X 宏定义内存池大小数组
static const size_t pool_sizes[] = {
#define X(size, count) size,
    CONFIG_FIXED_BLOCK_POOLS
#undef X
};

// 使用 X 宏定义内存池数量数组
static const size_t pool_counts[] = {
#define X(size, count) count,
    CONFIG_FIXED_BLOCK_POOLS
#undef X
};

// 使用 X 宏定义内存池内存指针数组
static char *pool_memories[] = {
#define X(size, count) &mem_##size##_##count[0][0],
    CONFIG_FIXED_BLOCK_POOLS
#undef X
};

// 全局内存分配器实例
static fixed_block_allocator_t g_fixed_block_allocator;
// 初始化标志，确保只初始化一次
static int g_initialized = 0;

// 内存分配器初始化函数
void fixed_block_allocator_init(void) {
    // 加锁保护
    FIXED_BLOCK_LOCK();
    
    // 检查是否已经初始化
    if (g_initialized) {
        // 解锁
        FIXED_BLOCK_UNLOCK();
        return;
    }
    
    g_fixed_block_allocator.pool_count = POOL_COUNT;
    
    // 初始化每个内存池
    for (size_t i = 0; i < POOL_COUNT; i++) {
        fixed_block_pool_t *pool = &g_fixed_block_allocator.pools[i];
        pool->block_size = pool_sizes[i];
        pool->block_count = pool_counts[i];
        pool->used_count = 0;
        pool->max_used_count = 0;
        pool->free_list = NULL;
        pool->memory = pool_memories[i];
        
        // 初始化空闲内存块链表
        for (size_t j = 0; j < pool->block_count; j++) {
            fixed_block_t *block = (fixed_block_t *)(pool->memory + j * pool->block_size);
            block->next = pool->free_list;
            block->pool_size = pool->block_size;
            pool->free_list = block;
        }
    }
    
    // 标记初始化完成
    g_initialized = 1;
    
    // 解锁
    FIXED_BLOCK_UNLOCK();
}

// 内存分配函数
void *fixed_block_alloc(size_t size) {
    // 加锁保护
    FIXED_BLOCK_LOCK();
    
    // 找到合适的内存池
    size_t pool_index = 0;
    void *result = NULL;
    while (pool_index < g_fixed_block_allocator.pool_count) {
        fixed_block_pool_t *pool = &g_fixed_block_allocator.pools[pool_index];
        if (pool->block_size >= size) {
            // 检查该内存池是否有空闲块
            if (pool->free_list) {
                // 从空闲链表中取出一个内存块
                fixed_block_t *block = pool->free_list;
                pool->free_list = block->next;
                
                // 更新使用计数
                pool->used_count++;
                if (pool->used_count > pool->max_used_count) {
                    pool->max_used_count = pool->used_count;
                }
                
                // 返回内存块的指针（跳过 fixed_block_t 头）
                result = (void *)(block + 1);
                break;
            }
        }
        pool_index++;
    }
    
    // 解锁
    FIXED_BLOCK_UNLOCK();
    
    // 没有找到合适的内存池
    return result;
}

// 内存释放函数
void fixed_block_free(void *ptr) {
    if (!ptr) {
        return;
    }
    
    // 加锁保护
    FIXED_BLOCK_LOCK();
    
    // 找到内存块的头部
    fixed_block_t *block = (fixed_block_t *)ptr - 1;
    size_t pool_size = block->pool_size;
    
    // 找到对应的内存池
    for (size_t i = 0; i < g_fixed_block_allocator.pool_count; i++) {
        fixed_block_pool_t *pool = &g_fixed_block_allocator.pools[i];
        if (pool->block_size == pool_size) {
            // 将内存块放回空闲链表
            block->next = pool->free_list;
            pool->free_list = block;
            
            // 更新使用计数
            if (pool->used_count > 0) {
                pool->used_count--;
            }
            break;
        }
    }
    
    // 解锁
    FIXED_BLOCK_UNLOCK();
}

// 内存使用统计函数
void fixed_block_allocator_stats(void) {
    // 加锁保护
    FIXED_BLOCK_LOCK();
    
    printf("Fixed Block Allocator Stats:\n");
    for (size_t i = 0; i < g_fixed_block_allocator.pool_count; i++) {
        fixed_block_pool_t *pool = &g_fixed_block_allocator.pools[i];
        printf("Pool size: %zu bytes, Total blocks: %zu, Used blocks: %zu, Max used: %zu\n",
               pool->block_size, pool->block_count, pool->used_count, pool->max_used_count);
    }
    
    // 解锁
    FIXED_BLOCK_UNLOCK();
}
