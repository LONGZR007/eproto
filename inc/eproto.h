#ifndef EPROTO_H
#define EPROTO_H

#include <stdint.h>
#include "eproto_config.h"
#include "eproto_def.h"
#include "eproto_ring_buffer.h"
#include "eproto_list.h"
#include "eproto_packet_node.h"
#include "eproto_frame_parser.h"

// eProto - 嵌入式协议（Embedded Protocol）
// "e"代表嵌入式（Embedded），"Proto"代表协议（Protocol）

// 协议帧格式定义
// 帧结构：| 帧头(1) | 版本号(1) | 长度(2) | 包类型(1) | 原地址(1) | 设备地址(1)
// | 包ID(2) | 数据(n) | CRC(2) |

// 总线接口
typedef struct {
    void (*send)(uint8_t* data, uint16_t length);
    uint16_t (*receive)(uint8_t* buffer, uint16_t size);
} eproto_bus_t;

// 信号回调接口
typedef enum {
    EPROTO_SIGNAL_DATA = 0,    // 有数据
    EPROTO_SIGNAL_TIMEOUT,     // 超时
    EPROTO_SIGNAL_NO_PROGRESS  // 没有进展
} eproto_signal_result_t;

// 状态回调函数
typedef enum {
    EPROTO_STATUS_CRC_ERROR = 0,        // CRC校验错误
    EPROTO_STATUS_SLEEP_SUCCESS,        // 休眠成功
    EPROTO_STATUS_SLEEP_FAILED,         // 休眠失败
    EPROTO_STATUS_WAKEUP_SUCCESS,       // 唤醒成功
    EPROTO_STATUS_WAKEUP_FAILED,        // 唤醒失败
    EPROTO_STATUS_MULTIPLE_CRC_ERRORS,  // 多次连续CRC错误
    EPROTO_STATUS_HANDSHAKE_SUCCESS     // 握手成功
} eproto_status_t;

// 回调函数类型定义
typedef void (*eproto_status_callback_t)(eproto_status_t status, uint8_t* data, uint16_t length);
typedef void (*receive_callback_t)(uint8_t source_address, uint16_t packet_id, uint8_t* data, uint16_t length);
typedef void (*eproto_handshake_callback_t)(void);

// 设备队列结构体
typedef struct {
    struct eproto_list_head send_queue;  // 发送队列
    struct eproto_list_head wait_queue;  // 等待应答队列
} eproto_device_queues_t;

// 总线管理结构体
typedef struct {
    eproto_bus_t* bus;               // 总线接口
    eproto_ring_buffer_t rx_buffer;  // 接收环形缓冲区
    uint8_t self_addr;               // 对应的设备地址
    const char* name;                // 总线名称，用于日志和调试

    // 帧解析器
    eproto_frame_parser_t parser;
    // 接口函数
    eproto_status_callback_t status_callback;
    receive_callback_t receive_callback;
    // 状态变量
    uint16_t next_packet_id;
    uint16_t last_id;  // 上次处理的包ID，用于重发包检测
    uint8_t crc_error_count;
    eproto_node_t* current_send_node;  // 当前正在发送的节点
#ifdef EPROTO_ENABLE_HANDSHAKE
    // 握手相关
    eproto_handshake_callback_t handshake_callback;
    uint8_t handshake_required;  // 握手标志
#endif
    // 设备队列
    eproto_device_queues_t device_queues;
    // 目标设备地址数组
    uint8_t destination_devices[EPROTO_MAX_DESTINATION_DEVICES];
    // 目标设备地址数量
    uint8_t destination_device_count;
} eproto_bus_manager_t;

// 用户接口结构体
typedef struct {
    // 内存分配接口
    void* (*malloc)(size_t size);
    void (*free)(void* ptr);

    // 信号回调接口
    eproto_signal_result_t (*signal_wait)(uint32_t timestamp);
    void (*signal_send)(void);

    // 线程安全锁接口
    void (*lock)(void);
    void (*unlock)(void);

    // 1ms时间戳接口
    uint32_t (*get_timestamp)(void);

    // 超时时间戳
    uint32_t timeout_timestamp;
} eproto_user_functions_t;

// 错误码定义
typedef enum {
    EPROTO_OK = 0,
    EPROTO_ERROR_CRC,
    EPROTO_ERROR_TIMEOUT,
    EPROTO_ERROR_BUFFER_FULL,
    EPROTO_ERROR_INVALID_FRAME,
    EPROTO_ERROR_MAX_RETRY,
    EPROTO_ERROR_ROUTE_NOT_FOUND,
    EPROTO_ERROR_SLEEP_FAILED,
    EPROTO_ERROR_WAKEUP_FAILED
} eproto_error_t;

// eProto实例结构体（支持多实例）
typedef struct {
    eproto_user_functions_t user_functions;  // 用户函数

    // 总线管理器
    eproto_bus_manager_t bus_managers[EPROTO_MAX_BUS_COUNT];
} eproto_t;

/**
 * 初始化eProto实例
 * @param eproto           指向eProto实例的指针
 * @param user_functions   用户提供的回调函数集合
 * @return                 操作结果，EPROTO_OK表示成功，其他值表示错误
 */
eproto_error_t eproto_init(eproto_t* eproto, eproto_user_functions_t* user_functions);

/**
 * 销毁eProto实例
 * @param eproto   指向eProto实例的指针
 */
void eproto_destroy(eproto_t* eproto);

/**
 * 向eProto实例添加总线
 * @param eproto            指向eProto实例的指针
 * @param self_addr         总线的自身地址
 * @param bus               总线接口结构体
 * @param rx_buffer         接收缓冲区
 * @param rx_buffer_size    接收缓冲区大小
 * @param name              总线名称，用于日志和调试
 * @param handshake_callback 握手回调函数（仅当启用握手功能时有效）
 * @param status_callback   状态回调函数
* @param receive_callback   接收回调函数
 * @return                  操作结果，EPROTO_OK表示成功，其他值表示错误
 */
eproto_error_t eproto_add_bus(eproto_t* eproto, uint8_t self_addr, eproto_bus_t* bus, uint8_t* rx_buffer,
                              uint16_t rx_buffer_size, const char* name, eproto_handshake_callback_t handshake_callback,
                              eproto_status_callback_t status_callback, receive_callback_t receive_callback);

/**
 * 向指定总线添加目标设备地址
 * @param eproto             指向eProto实例的指针
 * @param bus_addr        总线地址
 * @param dst_addr 目标设备地址
 * @return                  操作结果，EPROTO_OK表示成功，其他值表示错误
 * @note                    当启用握手功能时，第一个添加的设备将被用于握手操作，后续添加的设备仅用于数据通信
 */
eproto_error_t eproto_add_destination_device(eproto_t* eproto, uint8_t self_addr, uint8_t dst_addr);

/**
 * 主动发送数据
 * @param eproto                指向eProto实例的指针
 * @param dst_addr   目标设备地址
 * @param data                  要发送的数据
 * @param length                数据长度
 * @param callback              发送完成后的回调函数
 * @param private_data          回调函数的私有数据
 * @param need_reply            是否需要等待回复（1表示需要，0表示不需要）
 * @return                      操作结果，EPROTO_OK表示成功，其他值表示错误
 * @note                data
 * 会被内部复制到分配的内存中，用户可以在调用后释放原始数据
 */
eproto_error_t eproto_send(eproto_t* eproto, uint8_t dst_addr, uint8_t* data, uint16_t length,
                           eproto_packet_callback_t callback, void* private_data, uint8_t need_reply);

/**
 * 发送用户回复包
 * @param eproto                指向eProto实例的指针
 * @param dst_addr   目标设备地址
 * @param packet_id             包ID
 * @param data                  要发送的数据
 * @param length                数据长度
 * @return                      操作结果，EPROTO_OK表示成功，其他值表示错误
 * @note                data
 * 会被内部复制到分配的内存中，用户可以在调用后释放原始数据
 */
eproto_error_t eproto_send_user_reply(eproto_t* eproto, uint8_t dst_addr, uint16_t packet_id, uint8_t* data,
                                      uint16_t length);

/**
 * 主动发送数据（扩展接口，支持自定义超时时间和最大重发次数）
 * @param eproto                指向eProto实例的指针
 * @param dst_addr   目标设备地址
 * @param data                  要发送的数据
 * @param length                数据长度
 * @param callback              发送完成后的回调函数
 * @param private_data          回调函数的私有数据
 * @param need_reply            是否需要等待回复（1表示需要，0表示不需要）
 * @param max_retry_count       最大重发次数
 * @param timeout_ms            超时时间（毫秒）
 * @return                      操作结果，EPROTO_OK表示成功，其他值表示错误
 * @note                    data
 * 会被内部复制到分配的内存中，用户可以在调用后释放原始数据
 */
eproto_error_t eproto_send_ex(eproto_t* eproto, uint8_t dst_addr, uint8_t* data, uint16_t length,
                              eproto_packet_callback_t callback, void* private_data, uint8_t need_reply,
                              uint8_t max_retry_count, uint32_t timeout_ms);

/**
 * 发送用户回复包（扩展接口，支持自定义超时时间和最大重发次数）
 * @param eproto                指向eProto实例的指针
 * @param dst_addr   目标设备地址
 * @param packet_id             包ID
 * @param data                  要发送的数据
 * @param length                数据长度
 * @param max_retry_count       最大重发次数
 * @param timeout_ms            超时时间（毫秒）
 * @return                      操作结果，EPROTO_OK表示成功，其他值表示错误
 * @note                    data
 * 会被内部复制到分配的内存中，用户可以在调用后释放原始数据
 */
eproto_error_t eproto_send_user_reply_ex(eproto_t* eproto, uint8_t dst_addr, uint16_t packet_id, uint8_t* data,
                                         uint16_t length, uint8_t max_retry_count, uint32_t timeout_ms);

#ifdef EPROTO_ENABLE_HANDSHAKE
/**
 * 设置总线握手标志
 * @param eproto        指向eProto实例的指针
 * @param bus_addr   总线地址
 * @param required      是否需要握手（1需要，0不需要）
 * @return              操作结果，EPROTO_OK表示成功，其他值表示错误
 */
eproto_error_t eproto_set_handshake(eproto_t* eproto, uint8_t bus_addr, uint8_t required);

/**
 * 执行总线握手
 * @param eproto        指向eProto实例的指针
 * @param bus_addr   总线地址
 * @return              操作结果，EPROTO_OK表示成功，其他值表示错误
 */
eproto_error_t eproto_handshake(eproto_t* eproto, uint8_t bus_addr);
#endif

/**
 * 接收数据处理（由中断或轮询调用）
 * @param eproto       指向eProto实例的指针
 * @param bus_addr  总线地址
 * @param data         接收到的数据指针
 * @param len          接收到的数据长度
 */
void eproto_receive_data(eproto_t* eproto, uint8_t bus_addr, const uint8_t* data, size_t len);

/**
 * 等待信号
 * @param eproto   指向eProto实例的指针
 * @return         信号状态，0表示超时，1表示有信号
 */
uint8_t eproto_wait_for_signal(eproto_t* eproto);

/**
 * 处理函数
 * @param eproto   指向eProto实例的指针
 * @return         最小超时时间戳
 */
uint32_t eproto_process(eproto_t* eproto);

/**
 * 获取指定总线的状态
 * @param eproto       指向eProto实例的指针
 * @param bus_addr  总线地址
 * @return            状态值，0表示不需要握手，1表示需要握手
 */
uint8_t eproto_get_status(eproto_t* eproto, uint8_t bus_addr);

#endif  // EPROTO_H
