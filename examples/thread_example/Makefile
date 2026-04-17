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

# Executable name and output directory
TARGET = thread_example
BINDIR = ../../bin

# Build rules
all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) ../../src/eproto.c ../../src/eproto_crc16.c ../../src/eproto_packet_node.c ../../src/eproto_ring_buffer.c ../../src/eproto_frame_parser.c

%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(BINDIR)/$(TARGET)

.PHONY: all clean
