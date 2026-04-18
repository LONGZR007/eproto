CC = gcc
CFLAGS = -Wall -Wextra -g -I.

# 源文件目录
SRC_DIR = ../../src

# 头文件目录
INC_DIR = ../../inc

# 构建目录
BUILD_DIR = build

# 输出目录
BINDIR = bin

# eProto 核心源文件
EPROTO_SRCS = eproto.c eproto_crc16.c eproto_packet_node.c eproto_ring_buffer.c eproto_frame_parser.c
EPROTO_OBJS = $(addprefix $(BUILD_DIR)/, $(EPROTO_SRCS:.c=.o))

# 本地源文件
LOCAL_SRCS = ipc_common.c
LOCAL_OBJS = $(addprefix $(BUILD_DIR)/, $(LOCAL_SRCS:.c=.o))

# 目标程序
TARGETS = process_a process_b process_c process_d process_e
BIN_TARGETS = $(addprefix $(BINDIR)/, $(TARGETS))

all: $(BUILD_DIR) $(BIN_TARGETS)

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BINDIR)/process_a: process_a.c $(EPROTO_OBJS) $(LOCAL_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -o $@ $^ -pthread

$(BINDIR)/process_b: process_b.c $(EPROTO_OBJS) $(LOCAL_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -o $@ $^ -pthread

$(BINDIR)/process_c: process_c.c $(EPROTO_OBJS) $(LOCAL_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -o $@ $^ -pthread

$(BINDIR)/process_d: process_d.c $(EPROTO_OBJS) $(LOCAL_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -o $@ $^ -pthread

$(BINDIR)/process_e: process_e.c $(EPROTO_OBJS) $(LOCAL_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -o $@ $^ -pthread

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -c $< -o $@

$(BUILD_DIR)/%.o: %.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -I.. -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) $(BIN_TARGETS)