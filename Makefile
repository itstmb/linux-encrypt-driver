KERNELDIR = /usr/src/linux-2.4.18-14custom
include $(KERNELDIR)/.config
CFLAGS = -c -D__KERNEL__ -DMODULE -I$(KERNELDIR)/include -O -Wall
all: encdec.o
enc.o: encdec.c encdec.h
	gcc $(CFLAGS) encdec.c
