CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g
TARGET  = meu_cliente
SRCS    = $(wildcard src/*.c)

$(TARGET): $(SRCS) $(wildcard src/*.h)
	$(CC) $(CFLAGS) -o $@ $(SRCS)

clean:
	rm -f $(TARGET)

.PHONY: clean
