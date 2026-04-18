# Makefile for simple test

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -g -I../../inc -I.

# Linker flags
LDFLAGS = -pthread

# Source files
SRCS = simple_test.c

# Build directory
BUILDDIR = build

# Object files
OBJS = $(addprefix $(BUILDDIR)/, $(SRCS:.c=.o))

# Executable name and output directory
TARGET = simple_test
BINDIR = ../../bin

# Build rules
all: $(BINDIR)/$(TARGET)

$(BINDIR)/$(TARGET): $(BUILDDIR) $(OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDFLAGS) ../../src/eproto.c ../../src/eproto_crc16.c ../../src/eproto_packet_node.c ../../src/eproto_ring_buffer.c ../../src/eproto_frame_parser.c fixed_block_allocator.c

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

$(BUILDDIR)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILDDIR) $(BINDIR)/$(TARGET)

.PHONY: all clean
