CC = gcc
CFLAGS = -Wall -Wextra -g

# 源文件目录
SRC_DIR = ../src

# 头文件目录
INC_DIR = ../inc

# 构建目录
BUILD_DIR = build

SRCS = eproto.c crc16.c packet_node.c ring_buffer.c frame_parser.c
OBJS = $(addprefix $(BUILD_DIR)/, $(SRCS:.c=.o))

all: $(BUILD_DIR) example_threads process_a

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

example_threads: example_threads.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I. -o example_threads example_threads.c $(OBJS) -pthread

process_a: multi_process/process_a.c multi_process/ipc_common.c $(OBJS)
	$(CC) $(CFLAGS) -I$(INC_DIR) -I. -o multi_process/process_a multi_process/process_a.c multi_process/ipc_common.c $(OBJS) -pthread

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -I$(INC_DIR) -I. -c $< -o $@

clean:
	@rm -rf $(BUILD_DIR) example_threads multi_process/process_a

run_threads_example:
	./example_threads
