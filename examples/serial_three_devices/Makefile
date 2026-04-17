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

# Object files
OBJ_SERIAL = $(SRC_SERIAL:.c=.o)
EPROTO_OBJS = $(notdir $(EPROTO_SRCS:.c=.o))

# Target devices and output directory
DEVICES = device_a device_b device_c
BINDIR = ../../bin
TARGETS = $(addprefix $(BINDIR)/, $(DEVICES))

# Default target
.PHONY: all clean
all: $(TARGETS)

# Build eProto library objects
%.o: ../../src/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Build serial common module
$(OBJ_SERIAL): $(SRC_SERIAL)
	$(CC) $(CFLAGS) -c $< -o $@

# Build device A
$(BINDIR)/device_a: device_a.o bus_config_macro_a.o $(OBJ_SERIAL) $(EPROTO_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

device_a.o: $(SRC_DEVICE)
	$(CC) $(CFLAGS) -DDEVICE_ID=65 -c $< -o $@

bus_config_macro_a.o: $(SRC_BUS_CONFIG)
	$(CC) $(CFLAGS) -DBUS_CONFIGS="BUS_CONFIG(0x01, \"bus1\", 2, 0x02, 0x04, 0, 0)" -c $< -o $@

# Build device B
$(BINDIR)/device_b: device_b.o bus_config_macro_b.o $(OBJ_SERIAL) $(EPROTO_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

device_b.o: $(SRC_DEVICE)
	$(CC) $(CFLAGS) -DDEVICE_ID=66 -c $< -o $@

bus_config_macro_b.o: $(SRC_BUS_CONFIG)
	$(CC) $(CFLAGS) -DBUS_CONFIGS="BUS_CONFIG(0x02, \"bus1\", 1, 0x01, 0, 0, 0), BUS_CONFIG(0x03, \"bus2\", 1, 0x04, 0, 0, 0)" -c $< -o $@

# Build device C
$(BINDIR)/device_c: device_c.o bus_config_macro_c.o $(OBJ_SERIAL) $(EPROTO_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

device_c.o: $(SRC_DEVICE)
	$(CC) $(CFLAGS) -DDEVICE_ID=67 -c $< -o $@

bus_config_macro_c.o: $(SRC_BUS_CONFIG)
	$(CC) $(CFLAGS) -DBUS_CONFIGS="BUS_CONFIG(0x04, \"bus1\", 2, 0x02, 0x01, 0, 0)" -c $< -o $@

# Clean target
clean:
	rm -f $(TARGETS)
	rm -f *.o