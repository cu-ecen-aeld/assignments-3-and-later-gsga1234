# AESD Assignment 5 — aesdsocket (native and cross-compile)
CROSS_COMPILE ?=
CC := $(CROSS_COMPILE)gcc
CFLAGS := -Wall -Wextra -g

TARGET := aesdsocket
SRC := aesdsocket.c
OBJ := $(SRC:.c=.o)

.PHONY: default all clean

default: all

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJ) $(TARGET)
