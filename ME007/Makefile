# Makefile for compiling Kernel
# modules on the fly.

obj-m += distance_me007.o

KVERSION := $(shell uname -r)
INCLUDE += -I/usr/src/linux-$(KVERSION)/arch/arm/mach-omap2/include
INCLUDE += -I/usr/src/linux-$(KVERSION)/arch/arm/plat-omap/include

all:
	make -C /lib/modules/$(KVERSION)/build $(INCLUDE) M=$(PWD) modules
clean:
	make -C /lib/modules/$(KVERSION)/build $(INCLUDE) M=$(PWD) clean

