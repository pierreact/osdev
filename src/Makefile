# VPATH
#http://www.gnu.org/software/make/manual/make.html#Automatic-Variables

CC=gcc
AS=nasm
LD=ld

SOURCES=    kmain.o \
            system.ports.o \
            system.monitor.o \
            system.mem.o

CFLAGS=     -m64 \
            -ffreestanding \
            -nostdlib \
            -mcmodel=kernel \
            -mno-red-zone \
            -mno-mmx \
            -mno-sse \
            -mno-sse2 \
            -mno-sse3 \
            -mno-3dnow \
            -I include \
            -Wall \
            -O \
            -fstrength-reduce \
            -fomit-frame-pointer \
            -finline-functions \
    		-nostdinc

LDFLAGS=    -nostdlib \
            -nodefaultlibs \
            -Tlink.ld

ASFLAGS=    -felf64

#vpath %.h ../include
#vpath %.c .
#vpath %.s .


all: $(SOURCES) bootsector.bin link 

clean:
	-rm *.o kernel bootsector.bin

link:
	$(LD) $(LDFLAGS) -o kernel $(SOURCES)

bootsector.bin: bootsector.asm
	nasm -f bin -o bootsector.bin bootsector.asm

.s.o:
	$(AS) $(ASFLAGS) $<




