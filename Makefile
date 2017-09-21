
obj-m := gpio.o
KSRC := /home/desd/xeno_ws/elinux_bb_kernel/KERNEL

PWD := $(shell if [ "$$PWD" != "" ]; then echo $$PWD; else pwd; fi)
EXTRA_CFLAGS := -I$(KSRC)/include/xenomai -I$(KSRC)/include/xenomai/posix

all:
	$(MAKE) -C $(KSRC) SUBDIRS=$(PWD) modules ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf-
clean:
	$(MAKE) -C $(KSRC) SUBDIRS=$(PWD) clean
