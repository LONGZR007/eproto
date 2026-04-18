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

#include "eproto_ring_buffer.h"

// 初始化环形缓冲区
void eproto_ring_buffer_init(eproto_ring_buffer_t* rb, uint8_t* buffer, uint16_t size) {
    rb->buffer = buffer;
    rb->size = size;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

// 获取环形缓冲区中可用数据量
uint16_t eproto_ring_buffer_available(eproto_ring_buffer_t* rb) {
    if (!rb || !rb->buffer)
        return 0;
    return rb->count;
}

// 获取环形缓冲区中剩余空间
uint16_t eproto_ring_buffer_free(eproto_ring_buffer_t* rb) {
    if (!rb || !rb->buffer)
        return 0;
    return rb->size - rb->count;
}

// 写入数据到环形缓冲区
uint16_t eproto_ring_buffer_write(eproto_ring_buffer_t* rb, const uint8_t* data, uint16_t length) {
    if (!rb || !rb->buffer || !data)
        return 0;

    uint16_t written = 0;
    uint16_t free_space = eproto_ring_buffer_free(rb);
    uint16_t write_size = (length < free_space) ? length : free_space;

    while (written < write_size) {
        rb->buffer[rb->head] = data[written];
        rb->head = (rb->head + 1) % rb->size;
        rb->count++;
        written++;
    }

    return written;
}

// 从环形缓冲区读取数据
uint16_t eproto_ring_buffer_read(eproto_ring_buffer_t* rb, uint8_t* buffer, uint16_t size) {
    if (!rb || !rb->buffer || !buffer)
        return 0;

    uint16_t read = 0;
    uint16_t available = eproto_ring_buffer_available(rb);
    uint16_t read_size = (size < available) ? size : available;

    while (read < read_size) {
        buffer[read] = rb->buffer[rb->tail];
        rb->tail = (rb->tail + 1) % rb->size;
        rb->count--;
        read++;
    }

    return read;
}

// 从环形缓冲区中丢弃指定长度的数据
void eproto_ring_buffer_discard(eproto_ring_buffer_t* rb, uint16_t length) {
    if (!rb || !rb->buffer)
        return;

    uint16_t available = eproto_ring_buffer_available(rb);
    if (length > available)
        length = available;

    rb->tail = (rb->tail + length) % rb->size;
    rb->count -= length;
}

// 清空环形缓冲区
void eproto_ring_buffer_clear(eproto_ring_buffer_t* rb) {
    if (!rb)
        return;
    rb->head = 0;
    rb->tail = 0;
    rb->count = 0;
}

uint16_t eproto_ring_buffer_size(eproto_ring_buffer_t* rb) {
    if (!rb || !rb->buffer)
        return 0;
    return rb->size;
}
