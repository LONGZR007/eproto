# 编译选项
CC = gcc
CFLAGS = -I../../inc -I. -Wall -Wextra -std=c99
LDFLAGS = -lpthread

# 目标文件
TARGET = forward_callback_test

# 源文件
SRCS = main.c common.c device_a.c device_b.c device_c.c fixed_block_allocator.c

# 源文件（eproto 库）
EPROTO_SRCS = ../../src/eproto.c ../../src/eproto_crc16.c ../../src/eproto_packet_node.c ../../src/eproto_ring_buffer.c ../../src/eproto_frame_parser.c

# 默认目标
all: $(TARGET)

# 编译目标
$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) $(EPROTO_SRCS) $(LDFLAGS) -o $(TARGET)

# 清理目标
clean:
	rm -f $(TARGET)

# 运行测试
run:
	./$(TARGET)

# 构建并运行
build-run: all run
