CC = gcc
QEMU_ENV ?= 0
CFLAGS := $(strip $(if $(filter 1,$(QEMU_ENV)),-static,))
CFLAGS += -g

SOURCES := $(wildcard *.c)
TARGETS := $(patsubst %.c,%,$(SOURCES))

all: $(TARGETS)

.PHONY: clean

%: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	-rm -f $(TARGETS) *.o
