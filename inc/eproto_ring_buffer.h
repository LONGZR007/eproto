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
uint16_t eproto_ring_buffer_write(eproto_ring_buffer_t* rb, uint8_t* data, uint16_t length);
uint16_t eproto_ring_buffer_read(eproto_ring_buffer_t* rb, uint8_t* buffer, uint16_t size);
uint16_t eproto_ring_buffer_available(eproto_ring_buffer_t* rb);
uint16_t eproto_ring_buffer_free(eproto_ring_buffer_t* rb);
void eproto_ring_buffer_discard(eproto_ring_buffer_t* rb, uint16_t length);
void eproto_ring_buffer_clear(eproto_ring_buffer_t* rb);
uint16_t eproto_ring_buffer_size(eproto_ring_buffer_t* rb);

#endif  // EPROTO_RING_BUFFER_H
