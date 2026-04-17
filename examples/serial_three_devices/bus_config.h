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