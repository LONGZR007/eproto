# eProto API 参考

## 1. 数据类型

### 1.1 总线接口

```c
typedef struct {
    void (*send)(uint8_t* data, uint16_t length);
    uint16_t (*receive)(uint8_t* buffer, uint16_t size);
} eproto_bus_t;
```

- **send**：发送数据的回调函数
  - `data`：要发送的数据
  - `length`：数据长度

- **receive**：接收数据的回调函数
  - `buffer`：接收缓冲区
  - `size`：缓冲区大小
  - 返回值：实际接收到的数据长度

### 1.2 信号回调接口

```c
typedef enum {
    EPROTO_SIGNAL_DATA = 0,    // 有数据
    EPROTO_SIGNAL_TIMEOUT,     // 超时
    EPROTO_SIGNAL_NO_PROGRESS  // 没有进展
} eproto_signal_result_t;
```

### 1.3 状态回调函数

```c
typedef enum {
    EPROTO_STATUS_CRC_ERROR = 0,        // CRC校验错误
    EPROTO_STATUS_SLEEP_SUCCESS,        // 休眠成功
    EPROTO_STATUS_SLEEP_FAILED,         // 休眠失败
    EPROTO_STATUS_WAKEUP_SUCCESS,       // 唤醒成功
    EPROTO_STATUS_WAKEUP_FAILED,        // 唤醒失败
    EPROTO_STATUS_MULTIPLE_CRC_ERRORS,  // 多次连续CRC错误
    EPROTO_STATUS_HANDSHAKE_SUCCESS     // 握手成功
} eproto_status_t;
```

### 1.4 设备队列结构体

```c
typedef struct {
    struct eproto_list_head send_queue;  // 发送队列
    struct eproto_list_head wait_queue;  // 等待应答队列
} eproto_device_queues_t;
```

### 1.5 总线管理结构体

```c
typedef struct {
    eproto_bus_t* bus;               // 总线接口
    eproto_ring_buffer_t rx_buffer;  // 接收环形缓冲区
    uint8_t self_addr;            // 对应的设备地址
    const char* name;                // 总线名称，用于日志和调试

    // 帧解析器
    eproto_frame_parser_t parser;
    // 接口函数
    eproto_status_callback_t status_callback;
    void (*receive_callback)(uint8_t src_addr, uint16_t packet_id, uint8_t* data, uint16_t length);
    // 状态变量
    uint16_t next_packet_id;
    uint16_t last_id;  // 上次处理的包ID，用于重发包检测
    uint8_t crc_error_count;
    eproto_node_t* current_send_node;  // 当前正在发送的节点
#ifdef EPROTO_ENABLE_HANDSHAKE
    // 握手相关
    eproto_handshake_callback_t handshake_callback;
    uint8_t handshake_required;        // 握手标志
#endif
    // 设备队列
    eproto_device_queues_t device_queues;
    // 目标设备地址数组
    uint8_t destination_devices[EPROTO_MAX_DESTINATION_DEVICES];
    // 目标设备地址数量
    uint8_t destination_device_count;
} eproto_bus_manager_t;
```

### 1.6 用户接口结构体

```c
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
```

### 1.7 错误码定义

```c
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
```

### 1.8 eProto实例结构体

```c
typedef struct {
    eproto_user_functions_t user_functions;  // 用户函数

    // 总线管理器
    eproto_bus_manager_t bus_managers[EPROTO_MAX_BUS_COUNT];
} eproto_t;
```

## 2. 核心 API

### 2.1 初始化和销毁

#### eproto_init

```c
eproto_error_t eproto_init(eproto_t* eproto, eproto_user_functions_t* user_functions);
```

- **功能**：初始化eProto实例
- **参数**：
  - `eproto`：指向eProto实例的指针
  - `user_functions`：用户提供的回调函数集合
- **返回值**：操作结果，`EPROTO_OK`表示成功，其他值表示错误

#### eproto_destroy

```c
void eproto_destroy(eproto_t* eproto);
```

- **功能**：销毁eProto实例
- **参数**：
  - `eproto`：指向eProto实例的指针

### 2.2 总线管理

#### eproto_add_bus

```c
eproto_error_t eproto_add_bus(eproto_t* eproto, uint8_t self_addr, eproto_bus_t* bus, uint8_t* rx_buffer,
                              uint16_t rx_buffer_size, const char* name, eproto_handshake_callback_t handshake_callback,
                              eproto_status_callback_t status_callback);
```

- **功能**：向eProto实例添加总线
- **参数**：
  - `eproto`：指向eProto实例的指针
  - `self_addr`：总线的自身地址
  - `bus`：总线接口结构体
  - `rx_buffer`：接收缓冲区
  - `rx_buffer_size`：接收缓冲区大小
  - `name`：总线名称，用于日志和调试
  - `handshake_callback`：握手回调函数（仅当启用握手功能时有效）
  - `status_callback`：状态回调函数
- **返回值**：操作结果，`EPROTO_OK`表示成功，其他值表示错误

#### eproto_add_destination_device

```c
eproto_error_t eproto_add_destination_device(eproto_t* eproto, uint8_t self_addr, uint8_t dst_addr);
```

- **功能**：向指定总线添加目标设备地址
- **参数**：
  - `eproto`：指向eProto实例的指针
  - `self_addr`：总线的自身地址
  - `dst_addr`：目标设备地址
- **返回值**：操作结果，`EPROTO_OK`表示成功，其他值表示错误
- **注意**：当启用握手功能时，第一个添加的设备将被用于握手操作，后续添加的设备仅用于数据通信

### 2.3 数据发送

#### eproto_send

```c
eproto_error_t eproto_send(eproto_t* eproto, uint8_t dst_addr, uint8_t* data, uint16_t length,
                           eproto_packet_callback_t callback, void* private_data, uint8_t no_wait);
```

- **功能**：主动发送数据
- **参数**：
  - `eproto`：指向eProto实例的指针
  - `dst_addr`：目标设备地址
  - `data`：要发送的数据
  - `length`：数据长度
  - `callback`：发送完成后的回调函数
  - `private_data`：回调函数的私有数据
  - `no_wait`：是否不需要等待回复
- **返回值**：操作结果，`EPROTO_OK`表示成功，其他值表示错误
- **注意**：`data`会被内部复制到分配的内存中，用户可以在调用后释放原始数据

#### eproto_send_user_reply

```c
eproto_error_t eproto_send_user_reply(eproto_t* eproto, uint8_t dst_addr, uint16_t packet_id, uint8_t* data,
                                      uint16_t length);
```

- **功能**：发送用户回复包
- **参数**：
  - `eproto`：指向eProto实例的指针
  - `dst_addr`：目标设备地址
  - `packet_id`：包ID
  - `data`：要发送的数据
  - `length`：数据长度
- **返回值**：操作结果，`EPROTO_OK`表示成功，其他值表示错误
- **注意**：`data`会被内部复制到分配的内存中，用户可以在调用后释放原始数据

#### eproto_send_ex

```c
eproto_error_t eproto_send_ex(eproto_t* eproto, uint8_t dst_addr, uint8_t* data, uint16_t length,
                              eproto_packet_callback_t callback, void* private_data, uint8_t no_wait,
                              uint8_t max_retry_count, uint32_t timeout_ms);
```

- **功能**：主动发送数据（扩展接口，支持自定义超时时间和最大重发次数）
- **参数**：
  - `eproto`：指向eProto实例的指针
  - `dst_addr`：目标设备地址
  - `data`：要发送的数据
  - `length`：数据长度
  - `callback`：发送完成后的回调函数
  - `private_data`：回调函数的私有数据
  - `no_wait`：是否不需要等待回复
  - `max_retry_count`：最大重发次数
  - `timeout_ms`：超时时间（毫秒）
- **返回值**：操作结果，`EPROTO_OK`表示成功，其他值表示错误
- **注意**：`data`会被内部复制到分配的内存中，用户可以在调用后释放原始数据

#### eproto_send_user_reply_ex

```c
eproto_error_t eproto_send_user_reply_ex(eproto_t* eproto, uint8_t dst_addr, uint16_t packet_id, uint8_t* data,
                                         uint16_t length, uint8_t max_retry_count, uint32_t timeout_ms);
```

- **功能**：发送用户回复包（扩展接口，支持自定义超时时间和最大重发次数）
- **参数**：
  - `eproto`：指向eProto实例的指针
  - `dst_addr`：目标设备地址
  - `packet_id`：包ID
  - `data`：要发送的数据
  - `length`：数据长度
  - `max_retry_count`：最大重发次数
  - `timeout_ms`：超时时间（毫秒）
- **返回值**：操作结果，`EPROTO_OK`表示成功，其他值表示错误
- **注意**：`data`会被内部复制到分配的内存中，用户可以在调用后释放原始数据

### 2.4 握手功能

#### eproto_set_handshake

```c
eproto_error_t eproto_set_handshake(eproto_t* eproto, uint8_t bus_addr, uint8_t required);
```

- **功能**：设置总线握手标志
- **参数**：
  - `eproto`：指向eProto实例的指针
  - `bus_addr`：总线地址
  - `required`：是否需要握手（1需要，0不需要）
- **返回值**：操作结果，`EPROTO_OK`表示成功，其他值表示错误
- **注意**：仅当启用握手功能时有效

#### eproto_handshake

```c
eproto_error_t eproto_handshake(eproto_t* eproto, uint8_t bus_addr);
```

- **功能**：执行总线握手
- **参数**：
  - `eproto`：指向eProto实例的指针
  - `bus_addr`：总线地址
- **返回值**：操作结果，`EPROTO_OK`表示成功，其他值表示错误
- **注意**：仅当启用握手功能时有效

### 2.5 数据接收和处理

#### eproto_receive_data

```c
void eproto_receive_data(eproto_t* eproto, uint8_t bus_addr, const uint8_t* data, size_t len);
```

- **功能**：接收数据处理（由中断或轮询调用）
- **参数**：
  - `eproto`：指向eProto实例的指针
  - `bus_addr`：总线地址
  - `data`：接收到的数据指针
  - `len`：接收到的数据长度

#### eproto_wait_for_signal

```c
uint8_t eproto_wait_for_signal(eproto_t* eproto);
```

- **功能**：等待信号
- **参数**：
  - `eproto`：指向eProto实例的指针
- **返回值**：信号状态，0表示超时，1表示有信号

#### eproto_process

```c
uint32_t eproto_process(eproto_t* eproto);
```

- **功能**：处理函数
- **参数**：
  - `eproto`：指向eProto实例的指针
- **返回值**：最小超时时间戳

### 2.6 状态获取

#### eproto_get_status

```c
uint8_t eproto_get_status(eproto_t* eproto, uint8_t bus_addr);
```

- **功能**：获取指定总线的状态
- **参数**：
  - `eproto`：指向eProto实例的指针
  - `bus_addr`：总线地址
- **返回值**：状态值，0表示不需要握手，1表示需要握手

## 3. 回调函数类型

### 3.1 数据包回调函数

```c
typedef void (*eproto_packet_callback_t)(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length, void* private_data);
```

- **参数**：
  - `status`：发送状态
  - `packet_id`：包ID
  - `data`：发送的数据
  - `length`：数据长度
  - `private_data`：私有数据

### 3.2 状态回调函数

```c
typedef void (*eproto_status_callback_t)(eproto_status_t status, uint8_t* data, uint16_t length);
```

- **参数**：
  - `status`：状态码
  - `data`：状态相关数据
  - `length`：数据长度



### 3.4 握手回调函数

```c
typedef void (*eproto_handshake_callback_t)(void);
```

- **参数**：无

## 4. 配置选项

eProto 通过 `eproto_config.h` 文件提供了以下配置选项：

| 配置项 | 描述 | 默认值 |
|--------|------|--------|
| `EPROTO_MAX_BUS_COUNT` | 最大总线数量 | 1 |
| `EPROTO_MAX_DESTINATION_DEVICES` | 每个总线的最大目标设备数量 | 4 |
| `EPROTO_BUFFER_SIZE` | 缓冲区大小 | 256 |
| `EPROTO_MAX_PACKET_SIZE` | 最大数据包大小 | 256 |
| `EPROTO_DEFAULT_TIMEOUT` | 默认超时时间（毫秒） | 1000 |
| `EPROTO_DEFAULT_RETRY_COUNT` | 默认重发次数 | 3 |
| `EPROTO_ENABLE_HANDSHAKE` | 是否启用握手功能 | 0 |
| `EPROTO_CRC_ERROR_THRESHOLD` | CRC错误阈值 | 3 |

## 5. 使用示例

### 5.1 基本使用流程

```c
// 1. 定义用户函数
eproto_user_functions_t user_functions = {
    .malloc = my_malloc,
    .free = my_free,
    .signal_wait = my_signal_wait,
    .signal_send = my_signal_send,
    .lock = my_lock,
    .unlock = my_unlock,
    .get_timestamp = my_get_timestamp,
    .timeout_timestamp = 0
};

// 2. 初始化eProto实例
eproto_t eproto_inst;
eproto_error_t error = eproto_init(&eproto_inst, &user_functions);
if (error != EPROTO_OK) {
    // 处理错误
    return error;
}

// 3. 定义总线接口
eproto_bus_t bus = {
    .send = my_bus_send,
    .receive = my_bus_receive
};

// 4. 添加总线
uint8_t rx_buffer[256];
error = eproto_add_bus(&eproto_inst, 0x01, &bus, rx_buffer, sizeof(rx_buffer),
                       "my_bus", my_handshake_callback, my_status_callback, my_receive_callback);
if (error != EPROTO_OK) {
    // 处理错误
    return error;
}

// 5. 添加目标设备
error = eproto_add_destination_device(&eproto_inst, 0x01, 0x02);
if (error != EPROTO_OK) {
    // 处理错误
    return error;
}

// 6. 发送数据
uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
error = eproto_send(&eproto_inst, 0x02, data, sizeof(data), my_send_callback, NULL, 0);
if (error != EPROTO_OK) {
    // 处理错误
    return error;
}

// 7. 定期调用定时处理函数
while (1) {
    eproto_process(&eproto_inst);
    // 其他处理
}

// 8. 销毁eProto实例
eproto_destroy(&eproto_inst);
```

### 5.2 中断处理示例

```c
// UART中断处理函数
void uart_interrupt_handler(void) {
    uint8_t data = uart_read();
    eproto_receive_data(&eproto_inst, 0x01, &data, 1);
}

// 定时器中断处理函数
void timer_interrupt_handler(void) {
    eproto_process(&eproto_inst);
}
```

### 5.3 回调函数示例

```c
// 发送回调函数
void my_send_callback(eproto_send_status_t status, uint16_t packet_id, uint8_t* data, uint16_t length, void* private_data) {
    switch (status) {
        case EPROTO_SEND_SUCCESS:
            printf("Send success, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_TIMEOUT:
            printf("Send timeout, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_ERROR:
            printf("Send error, packet ID: %d\n", packet_id);
            break;
        case EPROTO_SEND_BUSY:
            printf("Send busy, packet ID: %d\n", packet_id);
            break;
    }
}

// 接收回调函数
void my_receive_callback(uint8_t src_addr, uint16_t packet_id, uint8_t* data, uint16_t length) {
    printf("Received data from device 0x%02X, packet ID: %d\n", src_addr, packet_id);
    // 处理接收到的数据
    // 可以调用 eproto_send_user_reply 发送回复
}

// 状态回调函数
void my_status_callback(eproto_status_t status, uint8_t* data, uint16_t length) {
    switch (status) {
        case EPROTO_STATUS_CRC_ERROR:
            printf("CRC error\n");
            break;
        case EPROTO_STATUS_HANDSHAKE_SUCCESS:
            printf("Handshake success\n");
            break;
        // 其他状态处理
    }
}
```

## 6. 错误处理

### 6.1 错误码表

| 错误码 | 描述 | 可能的原因 |
|--------|------|------------|
| `EPROTO_OK` | 成功 | 操作完成 |
| `EPROTO_ERROR_CRC` | CRC校验错误 | 数据传输错误 |
| `EPROTO_ERROR_TIMEOUT` | 超时 | 设备无响应 |
| `EPROTO_ERROR_BUFFER_FULL` | 缓冲区满 | 数据接收过快 |
| `EPROTO_ERROR_INVALID_FRAME` | 无效帧 | 帧格式错误 |
| `EPROTO_ERROR_MAX_RETRY` | 达到最大重发次数 | 设备持续无响应 |
| `EPROTO_ERROR_ROUTE_NOT_FOUND` | 路由未找到 | 目标设备不存在 |
| `EPROTO_ERROR_SLEEP_FAILED` | 休眠失败 | 设备无法休眠 |
| `EPROTO_ERROR_WAKEUP_FAILED` | 唤醒失败 | 设备无法唤醒 |

### 6.2 错误处理建议

1. **发送错误**：
   - 检查目标设备是否在线
   - 检查总线连接是否正常
   - 调整超时时间和重发次数

2. **接收错误**：
   - 检查CRC校验是否正确
   - 检查帧格式是否正确
   - 检查缓冲区大小是否足够

3. **握手错误**：
   - 检查设备是否支持握手功能
   - 检查设备地址是否正确
   - 检查总线连接是否正常

## 7. 注意事项

1. **内存管理**：
   - 确保提供的内存分配函数能够正确分配和释放内存
   - 避免内存泄漏，及时释放不再使用的内存

2. **时间管理**：
   - 确保时间戳函数返回的时间是准确的
   - 确保时间戳是单调递增的

3. **信号处理**：
   - 确保信号处理函数能够正确处理信号
   - 避免信号丢失或重复

4. **锁机制**：
   - 确保锁函数能够正确保护共享资源
   - 避免死锁和竞态条件

5. **总线接口**：
   - 确保总线发送和接收函数能够正确处理数据
   - 避免数据丢失或重复

6. **配置管理**：
   - 根据实际应用场景调整配置选项
   - 确保配置值合理，避免资源浪费或不足

## 8. 总结

本API参考文档提供了eProto协议库的详细API说明，包括数据类型、核心函数、回调函数和配置选项等。通过使用这些API，您可以在嵌入式系统中实现可靠的设备间通信。

在使用eProto时，建议：

1. 仔细阅读本参考文档，了解API的使用方法
2. 参考示例代码，学习如何使用eProto
3. 根据实际应用场景调整配置选项
4. 实现必要的回调函数，处理各种事件和状态变化
5. 进行充分的测试，确保通信的可靠性

通过正确使用eProto的API，您可以构建稳定、可靠的嵌入式通信系统。