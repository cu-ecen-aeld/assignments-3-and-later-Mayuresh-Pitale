
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -g -Wall -Werror
TARGET ?= aesdsocket
LDFLAGS ?= -lpthread -lrt

all: $(TARGET)
default: $(TARGET)

$(TARGET): aesdsocket.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

aesdsocket.o: aesdsocket.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(TARGET) *.o

.PHONY: all default clean