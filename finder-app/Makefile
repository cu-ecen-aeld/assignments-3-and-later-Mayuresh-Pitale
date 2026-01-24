#Author: Mayuresh Pitale
#Date: 2023-10-05
#reference: https://gemini.google.com/share/35faf02b45e3

CC = $(CROSS_COMPILE)gcc
CFLAGS = -g -Wall -Werror

TARGET = writer
SRCS = writer.c
OBJS = $(SRCS:.c=.o) # Convert source files to object files


all: $(TARGET)
# Build the target executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Compile source files into object files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all clean