# Makefile for serial three devices example

# Compiler and flags
CC ?= gcc
CFLAGS = -Wall -Wextra -I../../inc -I. -std=gnu99
LDFLAGS = -lpthread

# Source files
SRC_SERIAL = serial_common.c
SRC_DEVICE = device_macro.c
SRC_BUS_CONFIG = bus_config_macro.c

# EProto library source files
EPROTO_SRCS = \
	../../src/eproto.c \
	../../src/eproto_frame_parser.c \
	../../src/eproto_packet_node.c \
	../../src/eproto_ring_buffer.c \
	../../src/eproto_crc16.c

# Build directory
BUILDDIR = build
ARM_BUILDDIR = build-arm
BINDIR = bin
ARM_BINDIR = bin-arm

# Target devices
DEVICES = device_a device_b device_c

# Object files
OBJ_SERIAL = $(addprefix $(BUILDDIR)/, $(SRC_SERIAL:.c=.o))
ARM_OBJ_SERIAL = $(addprefix $(ARM_BUILDDIR)/, $(SRC_SERIAL:.c=.o))

EPROTO_OBJS = $(addprefix $(BUILDDIR)/, $(notdir $(EPROTO_SRCS:.c=.o)))
ARM_EPROTO_OBJS = $(addprefix $(ARM_BUILDDIR)/, $(notdir $(EPROTO_SRCS:.c=.o)))

DEVICE_OBJS = $(addprefix $(BUILDDIR)/, device_a.o device_b.o device_c.o bus_config_macro_a.o bus_config_macro_b.o bus_config_macro_c.o)
ARM_DEVICE_OBJS = $(addprefix $(ARM_BUILDDIR)/, device_a.o device_b.o device_c.o bus_config_macro_a.o bus_config_macro_b.o bus_config_macro_c.o)

TARGETS = $(addprefix $(BINDIR)/, $(DEVICES))
ARM_TARGETS = $(addprefix $(ARM_BINDIR)/, $(DEVICES))

# Default target
.PHONY: all arm clean clean-arm
all: $(TARGETS)

# Build for ARM target (requires CROSS_COMPILE variable)
# Usage: CROSS_COMPILE=/path/to/toolchain/arm-linux-gnueabihf- make arm
# Example: CROSS_COMPILE=/workspace/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf/bin/arm-linux-gnueabihf- make arm
arm: $(ARM_TARGETS)

# Create build directory
$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

$(ARM_BUILDDIR):
	@mkdir -p $(ARM_BUILDDIR)

# Build eProto library objects for x86
$(BUILDDIR)/eproto.o: ../../src/eproto.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/eproto_frame_parser.o: ../../src/eproto_frame_parser.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/eproto_packet_node.o: ../../src/eproto_packet_node.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/eproto_ring_buffer.o: ../../src/eproto_ring_buffer.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILDDIR)/eproto_crc16.o: ../../src/eproto_crc16.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build eProto library objects for ARM
$(ARM_BUILDDIR)/eproto.o: ../../src/eproto.c
	@mkdir -p $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

$(ARM_BUILDDIR)/eproto_frame_parser.o: ../../src/eproto_frame_parser.c
	@mkdir -p $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

$(ARM_BUILDDIR)/eproto_packet_node.o: ../../src/eproto_packet_node.c
	@mkdir -p $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

$(ARM_BUILDDIR)/eproto_ring_buffer.o: ../../src/eproto_ring_buffer.c
	@mkdir -p $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

$(ARM_BUILDDIR)/eproto_crc16.o: ../../src/eproto_crc16.c
	@mkdir -p $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

# Build serial common module for x86
$(OBJ_SERIAL): $(BUILDDIR)/serial_common.o

$(BUILDDIR)/serial_common.o: $(SRC_SERIAL)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build serial common module for ARM
$(ARM_OBJ_SERIAL): $(ARM_BUILDDIR)/serial_common.o

$(ARM_BUILDDIR)/serial_common.o: $(SRC_SERIAL)
	@mkdir -p $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -c $< -o $@

# Build device A (x86)
$(BINDIR)/device_a: $(BUILDDIR) $(BUILDDIR)/device_a.o $(BUILDDIR)/bus_config_macro_a.o $(OBJ_SERIAL) $(EPROTO_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(BUILDDIR)/device_a.o $(BUILDDIR)/bus_config_macro_a.o $(OBJ_SERIAL) $(EPROTO_OBJS) $(LDFLAGS)

$(BUILDDIR)/device_a.o: $(SRC_DEVICE)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -DDEVICE_ID=65 -c $< -o $@

$(BUILDDIR)/bus_config_macro_a.o: $(SRC_BUS_CONFIG)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -DBUS_CONFIGS="BUS_CONFIG(0x01, \"bus1\", 2, 0x02, 0x04, 0, 0)" -c $< -o $@

# Build device B (x86)
$(BINDIR)/device_b: $(BUILDDIR) $(BUILDDIR)/device_b.o $(BUILDDIR)/bus_config_macro_b.o $(OBJ_SERIAL) $(EPROTO_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(BUILDDIR)/device_b.o $(BUILDDIR)/bus_config_macro_b.o $(OBJ_SERIAL) $(EPROTO_OBJS) $(LDFLAGS)

$(BUILDDIR)/device_b.o: $(SRC_DEVICE)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -DDEVICE_ID=66 -c $< -o $@

$(BUILDDIR)/bus_config_macro_b.o: $(SRC_BUS_CONFIG)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -DBUS_CONFIGS="BUS_CONFIG(0x02, \"bus1\", 1, 0x01, 0, 0, 0), BUS_CONFIG(0x03, \"bus2\", 1, 0x04, 0, 0, 0)" -c $< -o $@

# Build device C (x86)
$(BINDIR)/device_c: $(BUILDDIR) $(BUILDDIR)/device_c.o $(BUILDDIR)/bus_config_macro_c.o $(OBJ_SERIAL) $(EPROTO_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(BUILDDIR)/device_c.o $(BUILDDIR)/bus_config_macro_c.o $(OBJ_SERIAL) $(EPROTO_OBJS) $(LDFLAGS)

$(BUILDDIR)/device_c.o: $(SRC_DEVICE)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -DDEVICE_ID=67 -c $< -o $@

$(BUILDDIR)/bus_config_macro_c.o: $(SRC_BUS_CONFIG)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -DBUS_CONFIGS="BUS_CONFIG(0x04, \"bus1\", 2, 0x03, 0x01, 0, 0)" -c $< -o $@

# Build device A (ARM)
$(ARM_BINDIR)/device_a: $(ARM_BUILDDIR) $(ARM_BUILDDIR)/device_a.o $(ARM_BUILDDIR)/bus_config_macro_a.o $(ARM_OBJ_SERIAL) $(ARM_EPROTO_OBJS)
	@mkdir -p $(ARM_BINDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -o $@ $(ARM_BUILDDIR)/device_a.o $(ARM_BUILDDIR)/bus_config_macro_a.o $(ARM_OBJ_SERIAL) $(ARM_EPROTO_OBJS) $(LDFLAGS)

$(ARM_BUILDDIR)/device_a.o: $(SRC_DEVICE)
	@mkdir -p $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -DDEVICE_ID=65 -c $< -o $@

$(ARM_BUILDDIR)/bus_config_macro_a.o: $(SRC_BUS_CONFIG)
	@mkdir -p $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -DBUS_CONFIGS="BUS_CONFIG(0x01, \"bus1\", 2, 0x02, 0x04, 0, 0)" -c $< -o $@

# Build device B (ARM)
$(ARM_BINDIR)/device_b: $(ARM_BUILDDIR) $(ARM_BUILDDIR)/device_b.o $(ARM_BUILDDIR)/bus_config_macro_b.o $(ARM_OBJ_SERIAL) $(ARM_EPROTO_OBJS)
	@mkdir -p $(ARM_BINDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -o $@ $(ARM_BUILDDIR)/device_b.o $(ARM_BUILDDIR)/bus_config_macro_b.o $(ARM_OBJ_SERIAL) $(ARM_EPROTO_OBJS) $(LDFLAGS)

$(ARM_BUILDDIR)/device_b.o: $(SRC_DEVICE)
	@mkdir -p $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -DDEVICE_ID=66 -c $< -o $@

$(ARM_BUILDDIR)/bus_config_macro_b.o: $(SRC_BUS_CONFIG)
	@mkdir -p $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -DBUS_CONFIGS="BUS_CONFIG(0x02, \"bus1\", 1, 0x01, 0, 0, 0), BUS_CONFIG(0x03, \"bus2\", 1, 0x04, 0, 0, 0)" -c $< -o $@

# Build device C (ARM)
$(ARM_BINDIR)/device_c: $(ARM_BUILDDIR) $(ARM_BUILDDIR)/device_c.o $(ARM_BUILDDIR)/bus_config_macro_c.o $(ARM_OBJ_SERIAL) $(ARM_EPROTO_OBJS)
	@mkdir -p $(ARM_BINDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -o $@ $(ARM_BUILDDIR)/device_c.o $(ARM_BUILDDIR)/bus_config_macro_c.o $(ARM_OBJ_SERIAL) $(ARM_EPROTO_OBJS) $(LDFLAGS)

$(ARM_BUILDDIR)/device_c.o: $(SRC_DEVICE)
	@mkdir -p $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -DDEVICE_ID=67 -c $< -o $@

$(ARM_BUILDDIR)/bus_config_macro_c.o: $(SRC_BUS_CONFIG)
	@mkdir -p $(ARM_BUILDDIR)
	$(CROSS_COMPILE)gcc $(CFLAGS) -DBUS_CONFIGS="BUS_CONFIG(0x04, \"bus1\", 2, 0x03, 0x01, 0, 0)" -c $< -o $@

# Clean targets
clean:
	rm -rf $(BUILDDIR) $(BINDIR)

clean-arm:
	rm -rf $(ARM_BUILDDIR) $(ARM_BINDIR)
