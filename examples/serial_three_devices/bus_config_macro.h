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

#ifndef BUS_CONFIG_MACRO_H
#define BUS_CONFIG_MACRO_H

#include "bus_config.h"

/* 宏定义总线配置
 * 用法：
 * #define BUS_CONFIGS \
 *     BUS_CONFIG(0x01, "bus1", 2, {0x02, 0x03}) \
 *     BUS_CONFIG(0x02, "bus2", 1, {0x04})
 */

// 总线配置宏
#define BUS_CONFIG(bus_addr, bus_name, target_cnt, target1, target2, target3, target4) \
    { bus_addr, bus_name, target_cnt, {target1, target2, target3, target4} }

// 生成总线配置数组
#ifdef BUS_CONFIGS
bus_config_t bus_configs[] = {
    BUS_CONFIGS
};

// 自动计算总线数量
int bus_count = sizeof(bus_configs) / sizeof(bus_config_t);
#endif /* BUS_CONFIGS */

#endif /* BUS_CONFIG_MACRO_H */