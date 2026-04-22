# 编译选项
CC = gcc
CFLAGS = -I../../inc -I. -Wall -Wextra -std=c99
LDFLAGS = -lpthread

# 目标文件
TARGET = forward_callback_test

# 源文件
SRCS = main.c common.c device_a.c device_b.c device_c.c

# 目标文件
OBJS = $(SRCS:.c=.o)

# 依赖库
LIB_EPROTO = ../../build/libeproto.a

# 默认目标
all: $(TARGET)

# 编译目标
$(TARGET): $(OBJS) $(LIB_EPROTO)
	$(CC) $(OBJS) $(LIB_EPROTO) $(LDFLAGS) -o $(TARGET)

# 编译源文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 清理目标
clean:
	rm -f $(OBJS) $(TARGET)

# 运行测试
run:
	./$(TARGET)

# 构建并运行
build-run: all run
