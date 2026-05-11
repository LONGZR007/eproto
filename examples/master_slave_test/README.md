# Master-Slave UDP 网络拓扑测试

## 概述

这个测试演示了一个主设备（M1）和三个从设备（S2、S3、S4）之间的UDP网络通信。
所有设备绑定到相同的IP地址和端口（127.0.0.1:8888），使用UDP协议进行通信。

## 网络架构

```
      M1 (Master) - UDP: 127.0.0.1:8888
     /  |  \
    /   |   \
  S2    S3   S4
(Slave)(Slave)(Slave)
```

## 设备配置

| 设备   | eProto 地址 | 说明       |
|--------|-------------|------------|
| M1     | 0x01        | 主设备     |
| S2     | 0x02        | 从设备2    |
| S3     | 0x03        | 从设备3    |
| S4     | 0x04        | 从设备4    |

## 通信特性

- 所有设备都绑定到相同的 UDP 地址和端口
- 使用 SO_REUSEADDR 和 SO_REUSEPORT 选项允许多个进程绑定到同一端口
- 所有设备都能收到所有消息
- 通过 eProto 协议中的地址字段进行消息过滤
- 从设备只处理发送给它们的消息
- 不支持握手

## 编译

### 编译

```bash
cd /workspace/examples/master_slave_test
make
```

这将编译生成以下可执行文件：
- bin/master_m1 - 主设备
- bin/slave_s2 - 从设备2
- bin/slave_s3 - 从设备3
- bin/slave_s4 - 从设备4

## 运行测试

### 使用测试脚本

```bash
cd /workspace/examples/master_slave_test
./test_communication.sh
```

测试脚本会自动启动所有设备，运行一段时间后停止并显示输出。

### 手动运行

1. 在多个终端中分别运行：
```bash
# 终端1 - 主设备
cd /workspace/examples/master_slave_test
./bin/master_m1

# 终端2 - 从设备2
cd /workspace/examples/master_slave_test
./bin/slave_s2

# 终端3 - 从设备3
cd /workspace/examples/master_slave_test
./bin/slave_s3

# 终端4 - 从设备4
cd /workspace/examples/master_slave_test
./bin/slave_s4
```

主设备支持以下命令：
- `send <device_addr> <need_reply> <data...> - 向指定从设备发送数据
- `broadcast <need_reply> <data...> - 向所有从设备广播数据
- `send_reply <data...> - 向最后一个发消息的设备回复
- `help - 显示帮助信息
- `quit - 退出

## 文件说明

```
master_slave_test/
├── Makefile              - 编译配置
├── README.md             - 本文档
├── eproto_config.h     - eProto配置
├── network_common.h   - UDP网络通信公共头文件
├── network_common.c   - UDP网络通信公共实现
├── master_m1.c       - 主设备实现
├── slave.c           - 从设备通用实现（通过编译宏决定地址）
└── test_communication.sh - 测试脚本
```

## 清理

```bash
make clean
```
