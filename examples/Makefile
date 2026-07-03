obj-m += hello-1.o
obj-m += hello-2.o
obj-m += hello-3.o
obj-m += hello-4.o
obj-m += hello-5.o
obj-m += hello-6.o
obj-m += startstop.o
startstop-objs := start.o stop.o
obj-m += chardev.o
obj-m += procfs1.o
obj-m += procfs2.o
obj-m += procfs3.o
obj-m += procfs4.o
obj-m += hello-sysfs.o
obj-m += hello-debugfs.o
obj-m += hello-debugfs-file.o
obj-m += sleep.o
obj-m += print_string.o
obj-m += kbleds.o
obj-m += sched.o
obj-m += chardev2.o
obj-m += syscall-steal.o
obj-m += intrpt.o
obj-m += completions.o
obj-m += example_tasklet.o
obj-m += devicemodel.o
obj-m += example_spinlock.o
obj-m += example_rwlock.o
obj-m += example_atomic.o
obj-m += example_mutex.o
obj-m += bottomhalf.o
obj-m += bh_threaded.o
obj-m += ioctl.o
obj-m += vinput.o
obj-m += vkbd.o
obj-m += static_key.o
obj-m += led.o
obj-m += dht11.o
obj-m += devicetree.o
obj-m += dma.o
obj-m += blkram.o
obj-m += vnetloop.o
obj-m += kmem_cache.o

KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(CURDIR)
CLANG_FORMAT ?= $(shell command -v clang-format-20 2>/dev/null || command -v clang-format 2>/dev/null)

ifeq ($(CONFIG_STATUS_CHECK_GCC),y)
CC=$(STATUS_CHECK_GCC)
ccflags-y += -fanalyzer
endif

all:
	$(MAKE) -C $(KDIR) CC=$(CC) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) CC=$(CC) M=$(PWD) clean
	$(RM) other/cat_nonblock *.plist

indent:
	@if [ -z "$(CLANG_FORMAT)" ]; then \
		echo "No clang-format found. Install clang-format-20 or clang-format." >&2; \
		exit 1; \
	fi
	"$(CLANG_FORMAT)" -i *.[ch]
	"$(CLANG_FORMAT)" -i other/*.[ch]
