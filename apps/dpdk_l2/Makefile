CC=gcc
LD=ld

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
        -I ../libc \
        -I ../../src/include

LDFLAGS= -nostdlib -T../link.ld

all: dpdk_l2

dpdk_l2: main.o ../libc/libc.a
	$(LD) $(LDFLAGS) -o dpdk_l2 main.o ../libc/libc.a

main.o: main.c
	$(CC) $(CFLAGS) -c -o main.o main.c

clean:
	-rm -f *.o dpdk_l2
