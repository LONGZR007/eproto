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

#ifndef BUS_CONFIG_H
#define BUS_CONFIG_H

#include <stdint.h>

// 最大目标设备数量
#define MAX_TARGETS_PER_BUS 4

// 总线配置结构体
typedef struct {
    uint8_t bus_address;      // 总线地址
    const char* bus_name;     // 总线名称
    uint8_t target_count;     // 目标设备数量
    uint8_t target_addresses[MAX_TARGETS_PER_BUS]; // 目标设备地址数组
} bus_config_t;

// 总线配置数组
extern bus_config_t bus_configs[];

// 总线数量
extern int bus_count;

#endif /* BUS_CONFIG_H */