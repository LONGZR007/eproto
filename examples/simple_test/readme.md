# simple_test 示例使用说明

## 示例概述

`simple_test` 是 eProto 协议库的一个基础示例，展示了两个设备之间的简单通信场景。该示例通过共享缓冲区模拟总线通信，实现了设备间的数据发送和接收功能。

### 主要功能

- 两个设备（Device 1 和 Device 2）之间的双向通信
- 点对点数据传输（需要回复）
- 广播数据传输（不需要回复）
- 数据接收后的自动回复
- 发送状态回调
- 状态变化通知

## 代码结构

```
simple_test/
├── Makefile              # 编译配置文件
├── eproto_config.h       # eProto 配置文件
├── fixed_block_allocator.c # 固定块内存分配器实现
├── fixed_block_allocator.h # 固定块内存分配器头文件
├── run_with_timeout.py   # 带超时的运行脚本
├── simple_test           # 编译生成的可执行文件
└── simple_test.c         # 示例主代码
```

## 编译步骤

1. **进入示例目录**：
   ```bash
   cd examples/simple_test
   ```

2. **编译示例**：
   ```bash
   make
   ```

   编译过程会生成 `simple_test` 可执行文件。

## 运行方式

### 直接运行

```bash
./simple_test
```

### 使用超时脚本运行

```bash
python3 run_with_timeout.py
```

这将在超时时间后自动终止程序，避免无限运行。

## 代码解析

### 核心组件

1. **设备模拟**：
   - `device1_thread`：模拟设备1的主线程
   - `device2_thread`：模拟设备2的主线程

2. **总线模拟**：
   - `device1_bus_send` / `device1_bus_receive`：设备1的总线发送和接收函数
   - `device2_bus_send` / `device2_bus_receive`：设备2的总线发送和接收函数
   - 使用共享缓冲区 `g_shared_buffer1` 和 `g_shared_buffer2` 模拟总线通信

3. **回调函数**：
   - `device1_receive_callback` / `device2_receive_callback`：数据接收回调
   - `device1_send_callback`：发送状态回调
   - `mock_status_callback`：状态变化回调

4. **线程**：
   - 每个设备都有接收线程和处理线程
   - 接收线程负责从总线读取数据并传递给 eProto 处理
   - 处理线程负责发送数据和调用 eProto 的定时处理函数

### 通信流程

1. **初始化**：
   - 两个设备分别初始化 eProto 实例
   - 添加总线和目标设备

2. **数据发送**：
   - 设备1发送测试数据到设备2（需要回复）
   - 设备1发送广播数据到所有设备（不需要回复）

3. **数据接收**：
   - 设备2接收设备1发送的数据
   - 自动回复接收到的数据
   - 设备1接收设备2的回复

4. **状态处理**：
   - 发送完成后通过回调函数通知发送状态
   - 状态变化通过状态回调函数通知

## 关键功能说明

### 1. 内存分配

示例提供了两种内存分配方式：
- 设备1使用 `fixed_block_allocator` 进行内存分配
- 设备2使用标准的 `malloc` 和 `free` 进行内存分配

### 2. 信号处理

使用 POSIX 信号量实现信号等待和发送功能：
- `device1_signal_wait` / `device1_signal_send`：设备1的信号处理
- `device2_signal_wait` / `device2_signal_send`：设备2的信号处理

### 3. 时间戳

使用 `gettimeofday` 实现时间戳获取功能，用于超时计算。

### 4. 锁机制

使用 pthread 互斥锁实现线程安全：
- 总线操作的互斥锁
- eProto 操作的互斥锁

## 运行结果

运行示例后，您将看到类似以下输出：

```
Simple Test: Two devices direct communication
=============================================

Device 1 thread started
Device 2 thread started
Device 1: eProto initialized successfully
Device 1: Bus added successfully
Device 1: Destination device 0x02 added successfully
Device 1 receive thread started
Device 1 process thread started
Device 2: eProto initialized successfully
Device 2: Bus added successfully
Device 2: Destination device 0x01 added successfully
Device 2 receive thread started
Device 2 process thread started
Device 1: Sending test data (needs reply)...
Device 1: Data sent successfully
Device 1 sent: AA 01 00 05 01 02 00 01 11 22 33 44 55 9F 7A 
Device 2 received 15 bytes: AA 01 00 05 01 02 00 01 11 22 33 44 55 9F 7A 
Device 2 received data from device 0x01, packet ID: 1: 11 22 33 44 55 
Device 2: Sending reply...
Device 2 sent: AA 01 00 05 02 01 00 01 11 22 33 44 55 6C E5 
Device 2: Reply sent successfully
Device 1 received 15 bytes: AA 01 00 05 02 01 00 01 11 22 33 44 55 6C E5 
Device 1 received data from device 0x02, packet ID: 1: 11 22 33 44 55 
Device 1: Sending reply...
Device 1 sent: AA 01 00 05 01 02 00 01 11 22 33 44 55 9F 7A 
Device 1: Reply sent successfully
Device 2 received 15 bytes: AA 01 00 05 01 02 00 01 11 22 33 44 55 9F 7A 
Device 2 received data from device 0x01, packet ID: 1: 11 22 33 44 55 
Device 2: Sending reply...
Device 2 sent: AA 01 00 05 02 01 00 01 11 22 33 44 55 6C E5 
Device 2: Reply sent successfully
Device 1: Send success, packet ID: 1
Device 1: Sending broadcast data to all devices...
Device 1: Broadcast data sent successfully
Device 1 sent: AA 01 00 05 01 FF 00 02 BB CC DD EE FF 63 78 
Device 2 received 15 bytes: AA 01 00 05 01 FF 00 02 BB CC DD EE FF 63 78 
Device 2 received data from device 0x01, packet ID: 2: BB CC DD EE FF 
Device 2: Sending reply...
Device 2 sent: AA 01 00 05 02 01 00 02 BB CC DD EE FF 27 61 
Device 2: Reply sent successfully
Device 1 received 15 bytes: AA 01 00 05 02 01 00 02 BB CC DD EE FF 27 61 
Device 1 received data from device 0x02, packet ID: 2: BB CC DD EE FF 
Device 1: Sending reply...
Device 1 sent: AA 01 00 05 01 02 00 02 BB CC DD EE FF B4 45 
Device 1: Reply sent successfully
Device 2 received 15 bytes: AA 01 00 05 01 02 00 02 BB CC DD EE FF B4 45 
Device 2 received data from device 0x01, packet ID: 2: BB CC DD EE FF 
Device 2: Sending reply...
Device 2 sent: AA 01 00 05 02 01 00 02 BB CC DD EE FF 27 61 
Device 2: Reply sent successfully
Device 1: Send success, packet ID: 2
Device 1 receive thread finished
Device 1 process thread finished
Device 1 thread finished
Device 2 receive thread finished
Device 2 process thread finished
Device 2 thread finished

All tests completed
```

## 注意事项

1. **编译环境**：需要支持 pthread 库的编译环境
2. **内存分配**：示例中设备1使用固定块内存分配器，设备2使用标准内存分配，可根据实际需求选择
3. **总线模拟**：使用共享缓冲区模拟总线通信，实际应用中需要替换为真实的硬件总线接口
4. **运行时间**：示例运行约2.5秒后自动结束，通过循环次数控制
5. **数据格式**：示例中使用十六进制格式打印数据，便于调试

## 扩展建议

1. **替换总线接口**：将共享缓冲区替换为实际的硬件总线，如 UART、SPI、I2C 等
2. **增加设备数量**：修改代码以支持更多设备之间的通信
3. **添加错误处理**：增强错误处理机制，提高系统稳定性
4. **优化内存使用**：根据实际硬件资源优化内存分配策略

## 相关文档

- [根目录 README.md](file:///workspace/README.md)：项目整体介绍
- [协议文档](file:///workspace/docs/PROTOCOL.md)：详细介绍协议帧格式和通信流程
- [API参考](file:///workspace/docs/API_REFERENCE.md)：详细的API使用说明