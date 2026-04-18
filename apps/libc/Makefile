CC=gcc

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
        -fno-PIC \
        -nostdinc \
        -I .

OBJECTS= stdio.o string.o net_eth.o net_arp.o net_pktbuf.o net_l2.o net_pktrace.o

all: libc.a

libc.a: $(OBJECTS)
	ar rcs libc.a $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	-rm -f *.o libc.a
