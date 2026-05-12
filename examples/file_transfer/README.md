# eProto 上层应用协议 - 文件传输 Demo

这是一个基于 eProto 协议的上层应用协议演示，专门用于文件传输功能。

## 协议设计概述

### 上层协议帧结构
```
| 功能码(1) | 标志位(1) | 数据长度(2) | 数据(n) |
```

### 功能码定义
- `0x01`: 文件传输启动 (FILE_START)
- `0x02`: 文件数据包 (FILE_DATA)
- `0x03`: 文件传输结束 (FILE_END)
- `0x04`: 文件传输确认 (FILE_ACK)

### 标志位定义
- `bit0`: 加密标志 (0=未加密, 1=加密)
- `bit1`: 需要回复标志 (0=不需要, 1=需要)
- `bit2-7`: 保留扩展

## 文件结构

- `eproto_app.h`: 上层协议头文件，包含协议结构定义和函数声明
- `eproto_app.c`: 上层协议实现文件
- `file_transfer_demo.c`: 演示程序
- `Makefile`: 编译配置

## 编译和运行

### 编译
```bash
make
```

### 运行
```bash
make run
```
或
```bash
./file_transfer_demo
```

### 清理
```bash
make clean
```

## 使用示例

### 发送文件
```c
// 1. 发送文件启动协议
uint8_t buffer[BUFFER_SIZE];
size_t len = eproto_app_pack_file_start(buffer, BUFFER_SIZE,
                                        EPROTO_APP_FLAG_NEED_REPLY,
                                        "test.txt", 8, 1024);

// 2. 发送文件数据包
for (uint32_t i = 0; i < num_packets; i++) {
    len = eproto_app_pack_file_data(buffer, BUFFER_SIZE, 0,
                                    i, data_ptr, data_len);
    // 发送...
}

// 3. 发送文件结束协议
len = eproto_app_pack_file_end(buffer, BUFFER_SIZE,
                               EPROTO_APP_FLAG_NEED_REPLY,
                               num_packets, 0);
```

### 接收文件
```c
// 解析协议头
eproto_app_header_t header;
eproto_app_parse_header(data, data_len, &header);

if (header.func_code == EPROTO_APP_FUNC_FILE_START) {
    // 解析文件信息
    eproto_app_parse_file_start(data + sizeof(eproto_app_header_t),
                                header.data_len,
                                &file_size, filename, sizeof(filename),
                                &filename_len);
} else if (header.func_code == EPROTO_APP_FUNC_FILE_DATA) {
    // 解析数据包
    eproto_app_parse_file_data(data + sizeof(eproto_app_header_t),
                               header.data_len,
                               &packet_index, &file_data, &data_len);
}
```

## 可扩展性

该协议设计具有良好的可扩展性：
- 可以添加新的功能码支持其他应用场景
- 标志位预留了扩展空间
- 数据字段格式可以根据功能码灵活定义
