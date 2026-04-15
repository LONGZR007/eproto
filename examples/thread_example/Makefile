# Makefile for thread example

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -g -I../../inc -I.

# Linker flags
LDFLAGS = -pthread

# Source files
SRCS = common.c device1.c device2.c device3.c main.c

# Object files
OBJS = $(SRCS:.c=.o)

# Dependencies
DEPS = common.h

# Executable name
TARGET = thread_example

# Build rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) ../../src/eproto.c ../../src/crc16.c ../../src/packet_node.c ../../src/ring_buffer.c ../../src/frame_parser.c

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
