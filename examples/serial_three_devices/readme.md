# serial_three_devices 示例使用说明

## 示例概述

`serial_three_devices` 是 eProto 协议库的一个串行三设备通信示例，展示了在串行总线拓扑下如何使用 eProto 进行设备间通信。该示例通过共享缓冲区模拟串行总线通信，实现了三个设备之间的串行通信功能。

### 主要功能

- 三个设备（Device 1、Device 2 和 Device 3）之间的串行通信
- 串行总线拓扑下的设备通信
- 设备间的数据发送和接收
- 发送状态回调
- 状态变化通知
- 总线配置和设备配置的宏定义

## 代码结构

```
serial_three_devices/
├── Makefile              # 编译配置文件
├── bus_config.h          # 总线配置头文件
├── bus_config_macro.c    # 总线配置宏实现
├── bus_config_macro.h    # 总线配置宏头文件
├── device_macro.c        # 设备宏实现
├── eproto_config.h       # eProto 配置文件
├── readme.md             # 本使用说明文档
├── serial_common.c       # 串行通信公共函数实现
└── serial_common.h       # 串行通信公共函数头文件
```

## 编译步骤

1. **进入示例目录**：
   ```bash
   cd examples/serial_three_devices
   ```

2. **编译示例**：
   ```bash
   make
   ```

   编译过程会生成相应的可执行文件。

## 运行方式

### 直接运行

```bash
./serial_three_devices
```

## 代码解析

### 核心组件

1. **串行通信公共组件**：
   - `serial_common.c` / `serial_common.h`：包含串行通信的公共函数，如发送和接收数据

2. **总线配置**：
   - `bus_config.h`：总线配置头文件
   - `bus_config_macro.c` / `bus_config_macro.h`：总线配置宏实现

3. **设备配置**：
   - `device_macro.c`：设备宏实现

### 通信拓扑

示例模拟了一个串行总线拓扑，其中：
- 设备1是主设备，连接到设备2
- 设备2是中间设备，连接到设备1和设备3
- 设备3是末端设备，连接到设备2

### 通信流程

1. **初始化**：
   - 三个设备分别初始化 eProto 实例
   - 配置总线和设备参数
   - 添加总线和目标设备

2. **数据发送**：
   - 设备1发送数据到设备2和设备3
   - 设备2发送数据到设备1和设备3
   - 设备3发送数据到设备1和设备2

3. **数据接收**：
   - 每个设备接收其他设备发送的数据
   - 自动回复接收到的数据
   - 中间设备负责转发数据

4. **状态处理**：
   - 发送完成后通过回调函数通知发送状态
   - 状态变化通过状态回调函数通知

## 关键功能说明

### 1. 串行总线模拟

示例使用共享缓冲区模拟串行总线通信：
- `serial_send` 函数：将数据写入共享缓冲区
- `serial_receive` 函数：从共享缓冲区读取数据
- 使用互斥锁保证总线操作的线程安全

### 2. 总线配置宏

使用宏定义简化总线配置：
- `BUS_CONFIG` 宏：配置总线参数
- `BUS_ADD_DEVICE` 宏：添加目标设备

### 3. 设备配置宏

使用宏定义简化设备配置：
- `DEVICE_CONFIG` 宏：配置设备参数
- `DEVICE_SEND_DATA` 宏：发送数据

### 4. 内存分配

使用标准的 `malloc` 和 `free` 进行内存分配，通过 `mock_malloc` 和 `mock_free` 函数封装。

### 5. 信号处理

使用 POSIX 信号量实现信号等待和发送功能：
- `signal_wait` 函数：等待信号或超时
- `signal_send` 函数：发送信号

### 6. 时间戳

使用 `gettimeofday` 实现时间戳获取功能，用于超时计算。

### 7. 锁机制

使用 pthread 互斥锁实现线程安全：
- 总线操作的互斥锁
- eProto 操作的互斥锁

## 运行结果

运行示例后，您将看到类似以下输出：

```
Serial Three Devices Example
============================

Device 1 thread started
Device 2 thread started
Device 3 thread started
Device 1: eProto initialized successfully
Device 1: Bus added successfully
Device 1: Destination device 0x02 added successfully
Device 1: Destination device 0x03 added successfully
Device 2: eProto initialized successfully
Device 2: Bus added successfully
Device 2: Destination device 0x01 added successfully
Device 2: Destination device 0x03 added successfully
Device 3: eProto initialized successfully
Device 3: Bus added successfully
Device 3: Destination device 0x01 added successfully
Device 3: Destination device 0x02 added successfully
Device 1: Sending data to device 0x02...
Device 1: Data sent successfully
Device 1: Sending data to device 0x03...
Device 1: Data sent successfully
Device 2: Sending data to device 0x01...
Device 2: Data sent successfully
Device 2: Sending data to device 0x03...
Device 2: Data sent successfully
Device 3: Sending data to device 0x01...
Device 3: Data sent successfully
Device 3: Sending data to device 0x02...
Device 3: Data sent successfully
Device 1: Send success, packet ID: 1
Device 1: Send success, packet ID: 2
Device 2: Send success, packet ID: 1
Device 2: Send success, packet ID: 2
Device 3: Send success, packet ID: 1
Device 3: Send success, packet ID: 2
Device 1 thread finished
Device 2 thread finished
Device 3 thread finished

All tests completed successfully
```

## 注意事项

1. **编译环境**：需要支持 pthread 库的编译环境
2. **内存分配**：示例使用标准的 `malloc` 和 `free` 进行内存分配
3. **总线模拟**：使用共享缓冲区模拟串行总线通信，实际应用中需要替换为真实的硬件总线接口
4. **运行时间**：示例运行约2.5秒后自动结束，通过循环次数控制
5. **数据格式**：示例中使用十六进制格式打印数据，便于调试
6. **多线程安全**：使用互斥锁保证多线程环境下的操作安全

## 扩展建议

1. **替换总线接口**：将共享缓冲区替换为实际的硬件串行总线，如 UART、RS-485 等
2. **增加设备数量**：修改代码以支持更多设备之间的串行通信
3. **添加错误处理**：增强错误处理机制，提高系统稳定性
4. **优化内存使用**：根据实际硬件资源优化内存分配策略
5. **添加更复杂的通信场景**：如设备发现、网络拓扑自动构建、数据路由等
6. **模拟不同的串行总线协议**：如 Modbus、CAN 等

## 相关文档

- [根目录 README.md](../../README.md)：项目整体介绍
- [协议文档](../../docs/PROTOCOL.md)：详细介绍协议帧格式和通信流程
- [API参考](../../docs/API_REFERENCE.md)：详细的API使用说明