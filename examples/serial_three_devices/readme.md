# 三设备串口通信示例

## 拓扑结构

```
|---- A ----|           |---- B ----|           |---- C ----|
|----------1|-----------|2---------3|-----------|4----------|
|-----------|           |-----------|           |-----------|
```

## 挂载关系

### 设备A挂载关系
| 端口 | 挂载到 | 说明 |
|------|--------|------|
| A1   | B2     | A到B的连接 |

### 设备B挂载关系
| 端口 | 挂载到 | 说明 |
|------|--------|------|
| B2   | A1     | B到A的连接 |
| B3   | C4     | B到C的连接 |

### 设备C挂载关系
| 端口 | 挂载到 | 说明 |
|------|--------|------|
| C4   | B3     | C到B的连接 |

## 编译说明

1. 进入示例目录：
   ```bash
   cd /workspace/eproto/examples/serial_three_devices
   ```

2. 编译所有设备：
   ```bash
   make
   ```

3. 清理编译产物：
   ```bash
   make clean
   ```

## 运行说明

### 设备A
```bash
./device_a <serial_port> <baud_rate>
```
- `<serial_port>`: 串口号，如 `/dev/ttyUSB0`
- `<baud_rate>`: 波特率，如 `115200`

### 设备B
```bash
./device_b <serial_port_a> <baud_rate_a> <serial_port_c> <baud_rate_c>
```
- `<serial_port_a>`: 连接到设备A的串口号
- `<baud_rate_a>`: 连接到设备A的波特率
- `<serial_port_c>`: 连接到设备C的串口号
- `<baud_rate_c>`: 连接到设备C的波特率

### 设备C
```bash
./device_c <serial_port> <baud_rate>
```
- `<serial_port>`: 串口号，如 `/dev/ttyUSB2`
- `<baud_rate>`: 波特率，如 `115200`

## 命令说明

### 发送数据
```
send <device_addr> <need_reply> <data...>
```
- `<device_addr>`: 目标设备地址 (1: A, 2: B, 3: C)
- `<need_reply>`: 是否需要回复 (1: 需要, 0: 不需要)
- `<data...>`: 要发送的数据，以空格分隔的十六进制值

示例：
```
send 2 1 11 22 33
```

### 回复数据
```
send_reply <data...>
```
- `<data...>`: 要回复的数据，以空格分隔的十六进制值

### 帮助
```
help
```

### 退出
```
quit
```

## 设备地址

- 设备A: 0x01
- 设备B: 0x02
- 设备C: 0x04

## 注意事项

1. 确保串口连接正确：设备A <-> 设备B <-> 设备C
2. 使用相同的波特率配置
3. 运行设备时，建议在不同的终端窗口中启动
4. 设备B作为中间设备，负责转发设备A和设备C之间的数据