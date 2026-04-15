#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

// 环形缓冲区结构体
typedef struct {
    uint8_t* buffer;  // 缓冲区地址
    uint16_t size;    // 缓冲区大小
    uint16_t head;    // 头部索引
    uint16_t tail;    // 尾部索引
    uint16_t count;   // 数据数量
} ring_buffer_t;

// 环形缓冲区操作函数
void ring_buffer_init(ring_buffer_t* rb, uint8_t* buffer, uint16_t size);
uint16_t ring_buffer_write(ring_buffer_t* rb, uint8_t* data, uint16_t length);
uint16_t ring_buffer_read(ring_buffer_t* rb, uint8_t* buffer, uint16_t size);
uint16_t ring_buffer_available(ring_buffer_t* rb);
uint16_t ring_buffer_free(ring_buffer_t* rb);
void ring_buffer_discard(ring_buffer_t* rb, uint16_t length);
void ring_buffer_clear(ring_buffer_t* rb);
uint16_t ring_buffer_size(ring_buffer_t* rb);

#endif  // RING_BUFFER_H