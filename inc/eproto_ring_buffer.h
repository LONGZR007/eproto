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

#ifndef EPROTO_RING_BUFFER_H
#define EPROTO_RING_BUFFER_H

#include <stdint.h>

// 环形缓冲区结构体
typedef struct {
    uint8_t* buffer;  // 缓冲区地址
    uint16_t size;    // 缓冲区大小
    uint16_t head;    // 头部索引
    uint16_t tail;    // 尾部索引
    uint16_t count;   // 数据数量
} eproto_ring_buffer_t;

// 环形缓冲区操作函数
void eproto_ring_buffer_init(eproto_ring_buffer_t* rb, uint8_t* buffer, uint16_t size);
uint16_t eproto_ring_buffer_write(eproto_ring_buffer_t* rb, const uint8_t* data, uint16_t length);
uint16_t eproto_ring_buffer_read(eproto_ring_buffer_t* rb, uint8_t* buffer, uint16_t size);
uint16_t eproto_ring_buffer_available(eproto_ring_buffer_t* rb);
uint16_t eproto_ring_buffer_free(eproto_ring_buffer_t* rb);
void eproto_ring_buffer_discard(eproto_ring_buffer_t* rb, uint16_t length);
void eproto_ring_buffer_clear(eproto_ring_buffer_t* rb);
uint16_t eproto_ring_buffer_size(eproto_ring_buffer_t* rb);

#endif  // EPROTO_RING_BUFFER_H
