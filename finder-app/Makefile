
CC = $(CROSS_COMPILE)gcc
CFLAGS = -g -Wall -Werror

TARGET = writer
SRCS = writer.c
OBJS = $(SRCS:.c=.o)


all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^


%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean