CC ?= gcc
QEMU_ENV ?= 0
CFLAGS ?= -g
DEPFLAGS = -MMD -MP

ifeq ($(QEMU_ENV),1)
override CFLAGS += -static
endif

SOURCES := $(wildcard *.c)
TARGETS := $(patsubst %.c,%,$(SOURCES))
DEPS := $(patsubst %.c,%.d,$(SOURCES))
MODE_STAMP := .qemu-env

.PHONY: all clean FORCE_ENV_CHECK

all: $(TARGETS)

$(MODE_STAMP): FORCE_ENV_CHECK
	@if [ ! -f $@ ] || [ "$$(cat $@)" != "$(QEMU_ENV)" ]; then \
		echo "$(QEMU_ENV)" > $@; \
	fi

%: %.c $(MODE_STAMP)
	@echo "  CC      $<"
	@$(CC) $(CFLAGS) $(DEPFLAGS) -o $@ $<

clean:
	-rm -f $(TARGETS) $(DEPS) $(MODE_STAMP) *.o

-include $(DEPS)
