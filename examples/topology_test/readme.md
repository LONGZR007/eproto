# topology_test 示例使用说明

## 示例概述

`topology_test` 是 eProto 协议库的一个拓扑测试示例，展示了在复杂网络拓扑下如何使用 eProto 进行设备间通信。该示例通过进程间通信（IPC）模拟总线通信，实现了五个设备之间的复杂通信功能。

### 主要功能

- 五个设备（Process A、Process B、Process C、Process D 和 Process E）之间的通信
- 进程间通信（IPC）模拟总线通信
- 复杂网络拓扑下的设备通信
- 设备间的数据发送和接收
- 发送状态回调
- 状态变化通知

## 代码结构

```
topology_test/
├── Makefile              # 编译配置文件
├── eproto_config.h       # eProto 配置文件
├── ipc_common.c          # IPC 公共函数实现
├── ipc_common.h          # IPC 公共函数头文件
├── modify_files.py       # 文件修改脚本
├── process_a             # 编译生成的进程A可执行文件
├── process_a.c           # 进程A实现
├── process_b             # 编译生成的进程B可执行文件
├── process_b.c           # 进程B实现
├── process_c             # 编译生成的进程C可执行文件
├── process_c.c           # 进程C实现
├── process_d             # 编译生成的进程D可执行文件
├── process_d.c           # 进程D实现
├── process_e             # 编译生成的进程E可执行文件
├── process_e.c           # 进程E实现
├── readme.md             # 本使用说明文档
└── test_communication.sh # 通信测试脚本
```

## 编译步骤

1. **进入示例目录**：
   ```bash
   cd examples/topology_test
   ```

2. **编译示例**：
   ```bash
   make
   ```

   编译过程会生成 `process_a`、`process_b`、`process_c`、`process_d` 和 `process_e` 可执行文件。

## 运行方式

### 直接运行各个进程

可以单独运行各个进程，但需要手动管理进程间的通信。

### 使用测试脚本运行

```bash
./test_communication.sh
```

这将启动所有进程并测试它们之间的通信。

## 代码解析

### 核心组件

1. **IPC 公共组件**：
   - `ipc_common.c` / `ipc_common.h`：包含进程间通信的公共函数，如发送和接收数据

2. **设备实现**：
   - `process_a.c`：进程A的实现，包括初始化、发送和接收处理
   - `process_b.c`：进程B的实现，包括初始化、发送和接收处理
   - `process_c.c`：进程C的实现，包括初始化、发送和接收处理
   - `process_d.c`：进程D的实现，包括初始化、发送和接收处理
   - `process_e.c`：进程E的实现，包括初始化、发送和接收处理

3. **测试脚本**：
   - `test_communication.sh`：启动所有进程并测试它们之间的通信

### 通信拓扑

示例模拟了一个复杂的网络拓扑，其中：
- 进程A可以与进程B和进程C通信
- 进程B可以与进程A、进程C和进程D通信
- 进程C可以与进程A、进程B和进程E通信
- 进程D可以与进程B通信
- 进程E可以与进程C通信

## 详细拓扑结构

```
                        |---- A ----|
  |---------------------|1----------|
  |                     |----------2|---------------------|
  |                     |-----------|                     |
  |                                                       |
  |                                                       |
  |                                                       |
  |                                                       |
  |                                                       |
  |                                                       |
  |                                                       |
|-3-- B ----|           |---- C ----|           |---- D --8-|
|----------4|-----------|6----------|           |-----------|
|-----------|           |----------7|-----------|9----------|
|-----5-----|           |-----------|           |-----10----|
      |                                               |
      |                                               |
      |                                               |
      |                                               |
      |                                               |
      |                 |---- E ----|                 |
      |-----------------|11---------|                 |
                        |---------12|-----------------|
                        |-----------|
```

## 最短路径挂载关系

### 规则说明
1. 按最短路径列举映射关系
2. 一个设备只可以挂载一个设备的一个端口，不可以挂载两个及以上的端口
3. 反向关系自动建立，如A到C是1下面挂6，则C到A是6下面挂1

### 完整挂载关系

#### A 设备挂载关系
| 端口 | 挂载到 | 说明 |
|------|--------|------|
| A1   | B3     | A到B的最短路径 |
| A1   | C6     | A到C的最短路径 |
| A2   | D8     | A到D的最短路径 |
| A1   | E11    | A到E的最短路径（通过B3→B5→E11） |

#### B 设备挂载关系
| 端口 | 挂载到 | 说明 |
|------|--------|------|
| B3   | A1     | B到A的最短路径 |
| B4   | C6     | B到C的最短路径 |
| B5   | E11    | B到E的最短路径 |
| B3   | D8     | B到D的最短路径（通过A1→A2→D8） |

#### C 设备挂载关系
| 端口 | 挂载到 | 说明 |
|------|--------|------|
| C6   | A1     | C到A的最短路径 |
| C6   | B4     | C到B的最短路径 |
| C7   | D9     | C到D的最短路径 |
| C6   | E11    | C到E的最短路径（通过B4→B5→E11） |

#### D 设备挂载关系
| 端口 | 挂载到 | 说明 |
|------|--------|------|
| D8   | A2     | D到A的最短路径 |
| D9   | C7     | D到C的最短路径 |
| D8   | B3     | D到B的最短路径（通过A2→A1→B3） |
| D10  | E12    | D到E的最短路径 |

#### E 设备挂载关系
| 端口 | 挂载到 | 说明 |
|------|--------|------|
| E11  | B5     | E到B的最短路径 |
| E11  | A1     | E到A的最短路径（通过B5→B3→A1） |
| E12  | D10    | E到D的最短路径 |
| E11  | C6     | E到C的最短路径（通过B5→B4→C6） |

## 拓扑结论

所有映射关系均已按照最短路径原则建立，且每个设备端口只挂载到一个目标端口，确保了无回环路径。每个设备端口都有明确的路由路径，可以到达网络中的其他设备。

### 通信流程

1. **初始化**：
   - 五个进程分别初始化 eProto 实例
   - 添加总线和目标设备

2. **数据发送**：
   - 进程A发送数据到进程B和进程C
   - 进程B发送数据到进程A、进程C和进程D
   - 进程C发送数据到进程A、进程B和进程E
   - 进程D发送数据到进程B
   - 进程E发送数据到进程C

3. **数据接收**：
   - 每个进程接收其他进程发送的数据
   - 自动回复接收到的数据

4. **状态处理**：
   - 发送完成后通过回调函数通知发送状态
   - 状态变化通过状态回调函数通知

## 关键功能说明

### 1. 进程间通信（IPC）

示例使用 Unix 域套接字实现进程间通信：
- `ipc_send` 函数：通过套接字发送数据
- `ipc_receive` 函数：通过套接字接收数据

### 2. 内存分配

使用标准的 `malloc` 和 `free` 进行内存分配，通过 `mock_malloc` 和 `mock_free` 函数封装。

### 3. 信号处理

使用 POSIX 信号量实现信号等待和发送功能：
- `signal_wait` 函数：等待信号或超时
- `signal_send` 函数：发送信号

### 4. 时间戳

使用 `gettimeofday` 实现时间戳获取功能，用于超时计算。

### 5. 锁机制

使用 pthread 互斥锁实现线程安全：
- IPC 操作的互斥锁
- eProto 操作的互斥锁

## 运行结果

运行测试脚本后，您将看到类似以下输出：

```
Topology Test: Five devices communication
========================================

Starting process A...
Starting process B...
Starting process C...
Starting process D...
Starting process E...

Process A: eProto initialized successfully
Process A: Bus added successfully
Process A: Destination device 0x02 added successfully
Process A: Destination device 0x03 added successfully
Process B: eProto initialized successfully
Process B: Bus added successfully
Process B: Destination device 0x01 added successfully
Process B: Destination device 0x03 added successfully
Process B: Destination device 0x04 added successfully
Process C: eProto initialized successfully
Process C: Bus added successfully
Process C: Destination device 0x01 added successfully
Process C: Destination device 0x02 added successfully
Process C: Destination device 0x05 added successfully
Process D: eProto initialized successfully
Process D: Bus added successfully
Process D: Destination device 0x02 added successfully
Process E: eProto initialized successfully
Process E: Bus added successfully
Process E: Destination device 0x03 added successfully

Process A: Sending data to device 0x02...
Process A: Data sent successfully
Process A: Sending data to device 0x03...
Process A: Data sent successfully

Process B: Sending data to device 0x01...
Process B: Data sent successfully
Process B: Sending data to device 0x03...
Process B: Data sent successfully
Process B: Sending data to device 0x04...
Process B: Data sent successfully

Process C: Sending data to device 0x01...
Process C: Data sent successfully
Process C: Sending data to device 0x02...
Process C: Data sent successfully
Process C: Sending data to device 0x05...
Process C: Data sent successfully

Process D: Sending data to device 0x02...
Process D: Data sent successfully

Process E: Sending data to device 0x03...
Process E: Data sent successfully

Process A: Send success, packet ID: 1
Process A: Send success, packet ID: 2
Process B: Send success, packet ID: 1
Process B: Send success, packet ID: 2
Process B: Send success, packet ID: 3
Process C: Send success, packet ID: 1
Process C: Send success, packet ID: 2
Process C: Send success, packet ID: 3
Process D: Send success, packet ID: 1
Process E: Send success, packet ID: 1

All tests completed successfully
```

## 注意事项

1. **编译环境**：需要支持 pthread 库和 Unix 域套接字的编译环境
2. **内存分配**：示例使用标准的 `malloc` 和 `free` 进行内存分配
3. **IPC 模拟**：使用 Unix 域套接字模拟总线通信，实际应用中需要替换为真实的硬件总线接口
4. **运行时间**：示例运行约2.5秒后自动结束，通过循环次数控制
5. **数据格式**：示例中使用十六进制格式打印数据，便于调试
6. **多进程安全**：使用互斥锁保证多进程环境下的操作安全

## 扩展建议

1. **替换总线接口**：将 IPC 替换为实际的硬件总线，如 UART、SPI、I2C 等
2. **增加设备数量**：修改代码以支持更多设备之间的通信
3. **添加错误处理**：增强错误处理机制，提高系统稳定性
4. **优化内存使用**：根据实际硬件资源优化内存分配策略
5. **添加更复杂的通信场景**：如设备发现、网络拓扑自动构建、路由选择等
6. **模拟不同的网络拓扑**：测试不同网络拓扑下的通信性能和可靠性

## 相关文档

- [根目录 README.md](../../README.md)：项目整体介绍
- [协议文档](../../docs/PROTOCOL.md)：详细介绍协议帧格式和通信流程
- [API参考](../../docs/API_REFERENCE.md)：详细的API使用说明