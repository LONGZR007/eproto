# eProto - 嵌入式通信协议库

## 项目简介

eProto是一个轻量级、可移植的嵌入式通信协议库，专为嵌入式系统设计，提供可靠的数据传输机制。

- **"e"** 代表嵌入式（Embedded）
- **"Proto"** 代表协议（Protocol）

该协议库支持多设备通信、CRC校验、自动重发、超时处理等功能，适用于各种嵌入式系统中的设备间通信场景。

## 主要特性

- **轻量级设计**：占用资源少，适合资源受限的嵌入式系统
- **可移植性**：通过抽象层设计，支持各种硬件平台
- **多设备支持**：支持多个设备之间的通信
- **可靠传输**：内置CRC校验、自动重发、超时处理机制
- **灵活配置**：可根据具体应用场景进行配置
- **多总线支持**：支持多个总线同时工作
- **握手机制**：可选的设备握手功能，确保通信可靠性

## 目录结构

```
├── docs/            # 文档目录
│   └── PROTOCOL.md  # 协议文档
├── examples/        # 示例代码
│   ├── serial_three_devices/  # 串行三设备通信示例
│   ├── simple_test/           # 简单两设备通信测试
│   ├── thread_example/        # 多线程示例
│   └── topology_test/         # 拓扑测试
├── inc/             # 头文件目录
│   ├── eproto.h               # 主头文件
│   ├── eproto_config_example.h # 配置示例
│   ├── eproto_crc16.h         # CRC16校验
│   ├── eproto_def.h           # 定义
│   ├── eproto_frame_parser.h  # 帧解析器
│   ├── eproto_list.h          # 链表
│   ├── eproto_packet_node.h   # 数据包节点
│   └── eproto_ring_buffer.h   # 环形缓冲区
├── src/             # 源代码目录
│   ├── eproto.c               # 主实现
│   ├── eproto_crc16.c         # CRC16实现
│   ├── eproto_frame_parser.c  # 帧解析器实现
│   ├── eproto_packet_node.c   # 数据包节点实现
│   └── eproto_ring_buffer.c   # 环形缓冲区实现
├── .clang-format    # 代码格式化配置
├── .gitignore       # Git忽略文件
└── LICENSE          # 许可证文件
```

## 快速开始

### 1. 配置协议

复制 `inc/eproto_config_example.h` 为 `inc/eproto_config.h` 并根据需要修改配置项。

### 2. 初始化协议

```c
// 定义用户函数
proto_user_functions_t user_functions = {
    .malloc = my_malloc,
    .free = my_free,
    .signal_wait = my_signal_wait,
    .signal_send = my_signal_send,
    .lock = my_lock,
    .unlock = my_unlock,
    .get_timestamp = my_get_timestamp,
    .timeout_timestamp = 0
};

// 初始化eProto实例
eproto_t eproto_inst;
eproto_error_t error = eproto_init(&eproto_inst, &user_functions);
```

### 3. 添加总线

```c
// 定义总线接口
eproto_bus_t bus = {
    .send = my_bus_send
};

// 添加总线
error = eproto_add_bus(&eproto_inst, self_address, &bus, rx_buffer, sizeof(rx_buffer),
                       "my_bus", status_callback, receive_callback);
```

### 4. 添加目标设备

```c
// 添加目标设备
error = eproto_add_destination_device(&eproto_inst, bus_address, destination_address);
```

### 5. 发送数据

```c
// 发送数据
uint8_t data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
error = eproto_send(&eproto_inst, destination_address, data, sizeof(data),
                   send_callback, NULL, 1);
```

### 6. 接收数据处理

```c
// 在中断或轮询中调用
void uart_interrupt_handler(void) {
    uint8_t data = uart_read();
    eproto_receive_data(&eproto_inst, bus_address, &data, 1);
}
```

### 7. 定时处理

```c
// 定期调用
void timer_interrupt_handler(void) {
    eproto_process(&eproto_inst);
}
```

## 示例代码

项目提供了多个示例代码，展示了不同场景下的使用方法：

- [**simple_test**](examples/simple_test/readme.md)：简单的两设备通信测试
- [**thread_example**](examples/thread_example/readme.md)：多线程环境下的使用示例
- [**topology_test**](examples/topology_test/readme.md)：复杂网络拓扑测试
- [**serial_three_devices**](examples/serial_three_devices/readme.md)：串行三设备通信示例

详细使用说明请参考各示例目录下的readme.md文件。

## 移植指南

将eProto移植到您的项目中，需要以下步骤：

1. **配置协议**：根据您的硬件平台和需求修改配置文件
2. **实现硬件接口**：实现总线发送和接收函数
3. **实现系统接口**：实现内存分配、信号处理、锁机制等接口
4. **集成到项目**：将eProto的头文件和源文件添加到您的项目中

详细的移植指南请参考 [docs/PORTING_GUIDE.md](docs/PORTING_GUIDE.md)。

## 详细文档

- [协议文档](docs/PROTOCOL.md)：详细介绍协议帧格式和通信流程
- [实现文档](docs/IMPLEMENTATION.md)：介绍协议的实现思路和架构
- [API参考](docs/API_REFERENCE.md)：详细的API使用说明
- [移植指南](docs/PORTING_GUIDE.md)：详细的移植步骤和注意事项

## 许可证

本项目采用 MIT 许可证，详情请参考 [LICENSE](LICENSE) 文件。