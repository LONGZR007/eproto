# Makefile for serial three devices example

# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -I../../inc -I.
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

# Object files
OBJ_SERIAL = $(addprefix $(BUILDDIR)/, $(SRC_SERIAL:.c=.o))
EPROTO_OBJS = $(addprefix $(BUILDDIR)/, $(notdir $(EPROTO_SRCS:.c=.o)))
DEVICE_OBJS = $(addprefix $(BUILDDIR)/, device_a.o device_b.o device_c.o bus_config_macro_a.o bus_config_macro_b.o bus_config_macro_c.o)

# Target devices and output directory
DEVICES = device_a device_b device_c
BINDIR = ../../bin
TARGETS = $(addprefix $(BINDIR)/, $(DEVICES))

# Default target
.PHONY: all clean
all: $(TARGETS)

# Create build directory
$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

# Build eProto library objects
$(BUILDDIR)/%.o: ../../src/%.c
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build serial common module
$(OBJ_SERIAL): $(BUILDDIR)/serial_common.o

$(BUILDDIR)/serial_common.o: $(SRC_SERIAL)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Build device A
$(BINDIR)/device_a: $(BUILDDIR) $(BUILDDIR)/device_a.o $(BUILDDIR)/bus_config_macro_a.o $(OBJ_SERIAL) $(EPROTO_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(BUILDDIR)/device_a.o $(BUILDDIR)/bus_config_macro_a.o $(OBJ_SERIAL) $(EPROTO_OBJS) $(LDFLAGS)

$(BUILDDIR)/device_a.o: $(SRC_DEVICE)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -DDEVICE_ID=65 -c $< -o $@

$(BUILDDIR)/bus_config_macro_a.o: $(SRC_BUS_CONFIG)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -DBUS_CONFIGS="BUS_CONFIG(0x01, \"bus1\", 2, 0x02, 0x04, 0, 0)" -c $< -o $@

# Build device B
$(BINDIR)/device_b: $(BUILDDIR) $(BUILDDIR)/device_b.o $(BUILDDIR)/bus_config_macro_b.o $(OBJ_SERIAL) $(EPROTO_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(BUILDDIR)/device_b.o $(BUILDDIR)/bus_config_macro_b.o $(OBJ_SERIAL) $(EPROTO_OBJS) $(LDFLAGS)

$(BUILDDIR)/device_b.o: $(SRC_DEVICE)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -DDEVICE_ID=66 -c $< -o $@

$(BUILDDIR)/bus_config_macro_b.o: $(SRC_BUS_CONFIG)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -DBUS_CONFIGS="BUS_CONFIG(0x02, \"bus1\", 1, 0x01, 0, 0, 0), BUS_CONFIG(0x03, \"bus2\", 1, 0x04, 0, 0, 0)" -c $< -o $@

# Build device C
$(BINDIR)/device_c: $(BUILDDIR) $(BUILDDIR)/device_c.o $(BUILDDIR)/bus_config_macro_c.o $(OBJ_SERIAL) $(EPROTO_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $(BUILDDIR)/device_c.o $(BUILDDIR)/bus_config_macro_c.o $(OBJ_SERIAL) $(EPROTO_OBJS) $(LDFLAGS)

$(BUILDDIR)/device_c.o: $(SRC_DEVICE)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -DDEVICE_ID=67 -c $< -o $@

$(BUILDDIR)/bus_config_macro_c.o: $(SRC_BUS_CONFIG)
	@mkdir -p $(BUILDDIR)
	$(CC) $(CFLAGS) -DBUS_CONFIGS="BUS_CONFIG(0x04, \"bus1\", 2, 0x02, 0x01, 0, 0)" -c $< -o $@

# Clean target
clean:
	rm -rf $(BUILDDIR) $(TARGETS)