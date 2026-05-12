# MIT License
#
# Copyright (c) 2026 LONGZR007
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -pthread
INCLUDES = -I../../inc -I.
LIBS = -lpthread

SRC_DIR = .
OBJ_DIR = .

SOURCES = $(SRC_DIR)/main.c \
          $(SRC_DIR)/common.c \
          $(SRC_DIR)/device1.c \
          $(SRC_DIR)/device2.c \
          ../../src/eproto.c \
          ../../src/eproto_crc16.c \
          ../../src/eproto_frame_parser.c \
          ../../src/eproto_packet_node.c \
          ../../src/eproto_ring_buffer.c \
          ../../src/eproto_upper.c

OBJECTS = $(SOURCES:.c=.o)
TARGET = file_transfer_demo

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(INCLUDES) -o $(TARGET) $(SOURCES) $(LIBS)

clean:
	rm -f $(TARGET) $(wildcard *.o)

.PHONY: all clean
