CC ?= gcc
CFLAGS ?= -Wall -Wextra -O2
TARGET = file_transfer_demo
SOURCES = file_transfer_demo.c eproto_app.c
HEADERS = eproto_app.h

.PHONY: all clean run

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) -o $@ $(SOURCES)

clean:
	rm -f $(TARGET)

run: $(TARGET)
	./$(TARGET)
