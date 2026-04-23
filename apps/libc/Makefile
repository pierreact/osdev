CC=gcc

# libc is built in the same "core" phase as the kernel (invoked from
# src/Makefile). It compiles the SHARED net sources from ../../src/net
# into libc.a so apps and kernel run the same code without forks.
# The libc-only files (stdio.c, string.c, nic_backend.c,
# pktrace_adapter.c) live here.

CFLAGS= -m64 \
        -ffreestanding \
        -nostdlib \
        -mno-red-zone \
        -mno-mmx \
        -mno-sse \
        -mno-sse2 \
        -mno-sse3 \
        -mno-3dnow \
        -Wall \
        -O \
        -fomit-frame-pointer \
        -fno-stack-protector \
        -fno-PIC \
        -nostdinc \
        -I . \
        -I ../../src/include

# Local libc objects
LIBC_OBJS= stdio.o string.o nic_backend.o pktrace_adapter.o

# Shared net objects compiled from ../../src/net/ into local .o files
# (with net_ prefix to avoid clashing with kernel's own net/ objects)
NET_OBJS= net_eth.o net_arp.o net_l2.o net_pktbuf.o net_pktrace.o

OBJECTS= $(LIBC_OBJS) $(NET_OBJS)

all: libc.a

libc.a: $(OBJECTS)
	ar rcs libc.a $(OBJECTS)

# Local libc C files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Shared net sources: compile in-place from src/net/ with libc CFLAGS.
# The object file is placed locally (next to libc.a) so the archive
# layout looks unchanged.
net_eth.o:     ../../src/net/eth.c
	$(CC) $(CFLAGS) -c -o $@ $<

net_arp.o:     ../../src/net/arp.c
	$(CC) $(CFLAGS) -c -o $@ $<

net_l2.o:      ../../src/net/l2.c
	$(CC) $(CFLAGS) -c -o $@ $<

net_pktbuf.o:  ../../src/net/pktbuf.c
	$(CC) $(CFLAGS) -c -o $@ $<

net_pktrace.o: ../../src/net/pktrace.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -f *.o libc.a
