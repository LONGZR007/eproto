# Makefile for simple test

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -g -I../../inc -I.

# Linker flags
LDFLAGS = -pthread

# Source files
SRCS = simple_test.c

# Object files
OBJS = $(SRCS:.c=.o)

# Executable name
TARGET = simple_test

# Build rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) ../../src/eproto.c ../../src/crc16.c ../../src/packet_node.c ../../src/ring_buffer.c ../../src/frame_parser.c

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
