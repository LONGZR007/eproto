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
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) ../../src/eproto.c ../../src/eproto_crc16.c ../../src/eproto_packet_node.c ../../src/eproto_ring_buffer.c ../../src/eproto_frame_parser.c fixed_block_allocator.c

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
