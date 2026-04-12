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
        -fno-PIC \
        -nostdinc \
        -I ../libc

LDFLAGS= -nostdlib -T../link.ld

all: demo_app

demo_app: main.o ../libc/libc.a
	$(LD) $(LDFLAGS) -o demo_app main.o ../libc/libc.a

main.o: main.c
	$(CC) $(CFLAGS) -c -o main.o main.c

clean:
	-rm -f *.o demo_app
