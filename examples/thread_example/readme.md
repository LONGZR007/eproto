# thread_example 示例使用说明

## 示例概述

`thread_example` 是 eProto 协议库的一个多线程示例，展示了在多线程环境下如何使用 eProto 进行设备间通信。该示例通过共享缓冲区模拟总线通信，实现了三个设备之间的多线程通信功能。

### 主要功能

- 三个设备（Device 1、Device 2 和 Device 3）之间的通信
- 多线程环境下的 eProto 使用
- 设备间的数据发送和接收
- 发送状态回调
- 状态变化通知

## 代码结构

```
thread_example/
├── Makefile              # 编译配置文件
├── common.c              # 公共函数实现
├── common.h              # 公共函数头文件
├── device1.c             # 设备1实现
├── device2.c             # 设备2实现
├── device3.c             # 设备3实现
├── eproto_config.h       # eProto 配置文件
├── main.c                # 主函数
├── run_with_timeout.py   # 带超时的运行脚本
└── thread_example        # 编译生成的可执行文件
```

## 编译步骤

1. **进入示例目录**：
   ```bash
   cd examples/thread_example
   ```

2. **编译示例**：
   ```bash
   make
   ```

   编译过程会生成 `thread_example` 可执行文件。

## 运行方式

### 直接运行

```bash
./thread_example
```

### 使用超时脚本运行

```bash
python3 run_with_timeout.py
```

这将在超时时间后自动终止程序，避免无限运行。

## 代码解析

### 核心组件

1. **公共组件**：
   - `common.c` / `common.h`：包含公共函数，如内存分配、时间戳获取、锁机制等

2. **设备实现**：
   - `device1.c`：设备1的实现，包括初始化、发送和接收处理
   - `device2.c`：设备2的实现，包括初始化、发送和接收处理
   - `device3.c`：设备3的实现，包括初始化、发送和接收处理

3. **主函数**：
   - `main.c`：创建三个设备线程并等待它们完成

### 通信流程

1. **初始化**：
   - 三个设备分别初始化 eProto 实例
   - 添加总线和目标设备

2. **数据发送**：
   - 设备1发送数据到设备2
   - 设备2发送数据到设备3
   - 设备3发送数据到设备1

3. **数据接收**：
   - 每个设备接收其他设备发送的数据
   - 自动回复接收到的数据

4. **状态处理**：
   - 发送完成后通过回调函数通知发送状态
   - 状态变化通过状态回调函数通知

## 关键功能说明

### 1. 多线程实现

示例使用 pthread 库创建三个设备线程，每个线程模拟一个设备的行为：
- 每个设备线程都有自己的 eProto 实例
- 每个设备线程都有接收和处理逻辑

### 2. 总线模拟

使用共享缓冲区模拟总线通信：
- `bus_send` 函数：将数据写入共享缓冲区
- `bus_receive` 函数：从共享缓冲区读取数据
- 使用互斥锁保证总线操作的线程安全

### 3. 内存分配

使用标准的 `malloc` 和 `free` 进行内存分配，通过 `mock_malloc` 和 `mock_free` 函数封装。

### 4. 信号处理

使用 POSIX 信号量实现信号等待和发送功能：
- `signal_wait` 函数：等待信号或超时
- `signal_send` 函数：发送信号

### 5. 时间戳

使用 `gettimeofday` 实现时间戳获取功能，用于超时计算。

### 6. 锁机制

使用 pthread 互斥锁实现线程安全：
- 总线操作的互斥锁
- eProto 操作的互斥锁

## 运行结果

运行示例后，您将看到类似以下输出：

```
Thread Example: Three devices communication
==========================================

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
Device 1 sent: AA 01 00 05 01 02 00 01 01 02 03 04 05 4D 39 
Device 2 received 15 bytes: AA 01 00 05 01 02 00 01 01 02 03 04 05 4D 39 
Device 2 received data from device 0x01, packet ID: 1: 01 02 03 04 05 
Device 2: Sending reply...
Device 2 sent: AA 01 00 05 02 01 00 01 01 02 03 04 05 1E A8 
Device 2: Reply sent successfully
Device 1 received 15 bytes: AA 01 00 05 02 01 00 01 01 02 03 04 05 1E A8 
Device 1 received data from device 0x02, packet ID: 1: 01 02 03 04 05 
Device 1: Sending reply...
Device 1 sent: AA 01 00 05 01 02 00 01 01 02 03 04 05 4D 39 
Device 1: Reply sent successfully
Device 2 received 15 bytes: AA 01 00 05 01 02 00 01 01 02 03 04 05 4D 39 
Device 2 received data from device 0x01, packet ID: 1: 01 02 03 04 05 
Device 2: Sending reply...
Device 2 sent: AA 01 00 05 02 01 00 01 01 02 03 04 05 1E A8 
Device 2: Reply sent successfully
Device 1: Send success, packet ID: 1
Device 2: Sending data to device 0x03...
Device 2: Data sent successfully
Device 2 sent: AA 01 00 05 02 03 00 01 06 07 08 09 0A 7A E6 
Device 3 received 15 bytes: AA 01 00 05 02 03 00 01 06 07 08 09 0A 7A E6 
Device 3 received data from device 0x02, packet ID: 1: 06 07 08 09 0A 
Device 3: Sending reply...
Device 3 sent: AA 01 00 05 03 02 00 01 06 07 08 09 0A 4B 75 
Device 3: Reply sent successfully
Device 2 received 15 bytes: AA 01 00 05 03 02 00 01 06 07 08 09 0A 4B 75 
Device 2 received data from device 0x03, packet ID: 1: 06 07 08 09 0A 
Device 2: Sending reply...
Device 2 sent: AA 01 00 05 02 03 00 01 06 07 08 09 0A 7A E6 
Device 2: Reply sent successfully
Device 3 received 15 bytes: AA 01 00 05 02 03 00 01 06 07 08 09 0A 7A E6 
Device 3 received data from device 0x02, packet ID: 1: 06 07 08 09 0A 
Device 3: Sending reply...
Device 3 sent: AA 01 00 05 03 02 00 01 06 07 08 09 0A 4B 75 
Device 3: Reply sent successfully
Device 2: Send success, packet ID: 1
Device 3: Sending data to device 0x01...
Device 3: Data sent successfully
Device 3 sent: AA 01 00 05 03 01 00 01 0B 0C 0D 0E 0F F7 82 
Device 1 received 15 bytes: AA 01 00 05 03 01 00 01 0B 0C 0D 0E 0F F7 82 
Device 1 received data from device 0x03, packet ID: 1: 0B 0C 0D 0E 0F 
Device 1: Sending reply...
Device 1 sent: AA 01 00 05 01 03 00 01 0B 0C 0D 0E 0F C8 F1 
Device 1: Reply sent successfully
Device 3 received 15 bytes: AA 01 00 05 01 03 00 01 0B 0C 0D 0E 0F C8 F1 
Device 3 received data from device 0x01, packet ID: 1: 0B 0C 0D 0E 0F 
Device 3: Sending reply...
Device 3 sent: AA 01 00 05 03 01 00 01 0B 0C 0D 0E 0F F7 82 
Device 3: Reply sent successfully
Device 1 received 15 bytes: AA 01 00 05 03 01 00 01 0B 0C 0D 0E 0F F7 82 
Device 1 received data from device 0x03, packet ID: 1: 0B 0C 0D 0E 0F 
Device 1: Sending reply...
Device 1 sent: AA 01 00 05 01 03 00 01 0B 0C 0D 0E 0F C8 F1 
Device 1: Reply sent successfully
Device 3: Send success, packet ID: 1
Device 1 thread finished
Device 2 thread finished
Device 3 thread finished

All tests completed
```

## 注意事项

1. **编译环境**：需要支持 pthread 库的编译环境
2. **内存分配**：示例使用标准的 `malloc` 和 `free` 进行内存分配
3. **总线模拟**：使用共享缓冲区模拟总线通信，实际应用中需要替换为真实的硬件总线接口
4. **运行时间**：示例运行约2.5秒后自动结束，通过循环次数控制
5. **数据格式**：示例中使用十六进制格式打印数据，便于调试
6. **多线程安全**：使用互斥锁保证多线程环境下的操作安全

## 扩展建议

1. **替换总线接口**：将共享缓冲区替换为实际的硬件总线，如 UART、SPI、I2C 等
2. **增加设备数量**：修改代码以支持更多设备之间的通信
3. **添加错误处理**：增强错误处理机制，提高系统稳定性
4. **优化内存使用**：根据实际硬件资源优化内存分配策略
5. **添加更复杂的通信场景**：如设备发现、网络拓扑自动构建等

## 相关文档

- [根目录 README.md](../../README.md)：项目整体介绍
- [协议文档](../../docs/PROTOCOL.md)：详细介绍协议帧格式和通信流程
- [API参考](../../docs/API_REFERENCE.md)：详细的API使用说明