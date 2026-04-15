CC = gcc
CFLAGS = -Wall -Wextra -g

# 源文件目录
SRC_DIR = ../../src

# 头文件目录
INC_DIR = ../../inc

# 构建目录
BUILD_DIR = build

# eProto 核心源文件
EPROTO_SRCS = eproto.c crc16.c packet_node.c ring_buffer.c frame_parser.c
EPROTO_OBJS = $(addprefix $(BUILD_DIR)/, $(EPROTO_SRCS:.c=.o))

# 本地源文件
LOCAL_SRCS = ipc_common.c
LOCAL_OBJS = $(addprefix $(BUILD_DIR)/, $(LOCAL_SRCS:.c=.o))

# 目标程序
TARGETS = process_a process_b process_c

all: $(BUILD_DIR) $(TARGETS)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

process_a: process_a.c $(EPROTO_OBJS) $(LOCAL_OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -o $@ $^ -pthread

process_b: process_b.c $(EPROTO_OBJS) $(LOCAL_OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -o $@ $^ -pthread

process_c: process_c.c $(EPROTO_OBJS) $(LOCAL_OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -o $@ $^ -pthread

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) $(TARGETS)
