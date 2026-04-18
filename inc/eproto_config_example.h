#ifndef EPROTO_CONFIG_H
#define EPROTO_CONFIG_H

// eProto - 嵌入式协议（Embedded Protocol）
// "e"代表嵌入式（Embedded），"Proto"代表协议（Protocol）

// 协议配置参数

// 最大数据包长度
#define EPROTO_MAX_PACKET_LENGTH 256

// 重发配置
#define EPROTO_DEFAULT_MAX_RETRY_COUNT 3     // 默认最大重发次数
#define EPROTO_DEFAULT_RETRY_TIMEOUT_MS 100  // 默认超时时间

// 握手配置
#define EPROTO_ENABLE_HANDSHAKE 1           // 启用握手功能（1=启用，0=禁用）
#define EPROTO_HANDSHAKE_MAX_RETRY_COUNT 3  // 握手最大重发次数
#define EPROTO_HANDSHAKE_TIMEOUT_MS 1000    // 握手超时时间（毫秒）

// 环形缓冲区大小 - 由用户在初始化时提供

// 发送队列最大长度
#define EPROTO_MAX_SEND_QUEUE_SIZE 16

// 等待应答队列最大长度
#define EPROTO_MAX_WAIT_QUEUE_SIZE 16

// 总线数量
#define EPROTO_MAX_BUS_COUNT 16

// 单个总线下最多支持的目标设备数量
#define EPROTO_MAX_DESTINATION_DEVICES 16

// 超时时间计算系数
#define EPROTO_TIMEOUT_FACTOR 1.5

// 日志打印接口示例（由用户实现）
// #define EPROTO_DEBUG_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
// #define EPROTO_INFO_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
// #define EPROTO_WARNING_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)
// #define EPROTO_ERROR_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__)

#endif  // EPROTO_CONFIG_H
