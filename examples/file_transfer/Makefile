# Makefile for eProto file transfer (serial version)
# Supports both native (x86) and ARM cross compilation

# Compiler and flags
CC ?= gcc
CFLAGS = -Wall -Wextra -I../../inc -I. -std=gnu99 -O2 -g
LDFLAGS = -lpthread

# Source files
SRC_SERIAL = common_serial.c
SRC_SENDER = sender.c
SRC_RECEIVER = receiver.c

# EProto library source files
EPROTO_SRCS = \
	../../src/eproto.c \
	../../src/eproto_frame_parser.c \
	../../src/eproto_packet_node.c \
	../../src/eproto_ring_buffer.c \
	../../src/eproto_crc16.c \
	../../src/eproto_upper.c

# Build directories
BUILDDIR = build
ARM_BUILDDIR = build-arm
BINDIR = bin
ARM_BINDIR = bin-arm

# Targets
TARGETS = $(BINDIR)/file_transfer_sender $(BINDIR)/file_transfer_receiver
ARM_TARGETS = $(ARM_BINDIR)/file_transfer_sender $(ARM_BINDIR)/file_transfer_receiver

# Default target
.PHONY: all arm clean clean-arm

all: $(TARGETS)

# Build for ARM target (requires CROSS_COMPILE variable)
# Usage: CROSS_COMPILE=/path/to/toolchain/arm-linux-gnueabihf- make arm
# Example: CROSS_COMPILE=/usr/bin/arm-linux-gnueabihf- make arm
arm: $(ARM_TARGETS)

# Create directories
$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

$(ARM_BUILDDIR):
	@mkdir -p $(ARM_BUILDDIR)

$(BINDIR):
	@mkdir -p $(BINDIR)

$(ARM_BINDIR):
	@mkdir -p $(ARM_BINDIR)

# EProto library objects - x86
$(BUILDDIR)/eproto.o: ../../src/eproto.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/eproto_frame_parser.o: ../../src/eproto_frame_parser.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/eproto_packet_node.o: ../../src/eproto_packet_node.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/eproto_ring_buffer.o: ../../src/eproto_ring_buffer.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/eproto_crc16.o: ../../src/eproto_crc16.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/eproto_upper.o: ../../src/eproto_upper.c | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# EProto library objects - ARM
$(ARM_BUILDDIR)/eproto.o: ../../src/eproto.c | $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

$(ARM_BUILDDIR)/eproto_frame_parser.o: ../../src/eproto_frame_parser.c | $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

$(ARM_BUILDDIR)/eproto_packet_node.o: ../../src/eproto_packet_node.c | $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

$(ARM_BUILDDIR)/eproto_ring_buffer.o: ../../src/eproto_ring_buffer.c | $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

$(ARM_BUILDDIR)/eproto_crc16.o: ../../src/eproto_crc16.c | $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

$(ARM_BUILDDIR)/eproto_upper.o: ../../src/eproto_upper.c | $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

# Serial common module - x86
$(BUILDDIR)/common_serial.o: $(SRC_SERIAL) | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Serial common module - ARM
$(ARM_BUILDDIR)/common_serial.o: $(SRC_SERIAL) | $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

# Sender - x86
$(BINDIR)/file_transfer_sender: $(BUILDDIR)/sender.o $(BUILDDIR)/common_serial.o \
	$(BUILDDIR)/eproto.o $(BUILDDIR)/eproto_frame_parser.o \
	$(BUILDDIR)/eproto_packet_node.o $(BUILDDIR)/eproto_ring_buffer.o \
	$(BUILDDIR)/eproto_crc16.o $(BUILDDIR)/eproto_upper.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/sender.o: $(SRC_SENDER) | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Receiver - x86
$(BINDIR)/file_transfer_receiver: $(BUILDDIR)/receiver.o $(BUILDDIR)/common_serial.o \
	$(BUILDDIR)/eproto.o $(BUILDDIR)/eproto_frame_parser.o \
	$(BUILDDIR)/eproto_packet_node.o $(BUILDDIR)/eproto_ring_buffer.o \
	$(BUILDDIR)/eproto_crc16.o $(BUILDDIR)/eproto_upper.o | $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BUILDDIR)/receiver.o: $(SRC_RECEIVER) | $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Sender - ARM
$(ARM_BINDIR)/file_transfer_sender: $(ARM_BUILDDIR)/sender.o $(ARM_BUILDDIR)/common_serial.o \
	$(ARM_BUILDDIR)/eproto.o $(ARM_BUILDDIR)/eproto_frame_parser.o \
	$(ARM_BUILDDIR)/eproto_packet_node.o $(ARM_BUILDDIR)/eproto_ring_buffer.o \
	$(ARM_BUILDDIR)/eproto_crc16.o $(ARM_BUILDDIR)/eproto_upper.o | $(ARM_BINDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(ARM_BUILDDIR)/sender.o: $(SRC_SENDER) | $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

# Receiver - ARM
$(ARM_BINDIR)/file_transfer_receiver: $(ARM_BUILDDIR)/receiver.o $(ARM_BUILDDIR)/common_serial.o \
	$(ARM_BUILDDIR)/eproto.o $(ARM_BUILDDIR)/eproto_frame_parser.o \
	$(ARM_BUILDDIR)/eproto_packet_node.o $(ARM_BUILDDIR)/eproto_ring_buffer.o \
	$(ARM_BUILDDIR)/eproto_crc16.o $(ARM_BUILDDIR)/eproto_upper.o | $(ARM_BINDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(ARM_BUILDDIR)/receiver.o: $(SRC_RECEIVER) | $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

# Clean targets
clean:
	rm -rf $(BUILDDIR) $(BINDIR)

clean-arm:
	rm -rf $(ARM_BUILDDIR) $(ARM_BINDIR)

clean-all: clean clean-arm
