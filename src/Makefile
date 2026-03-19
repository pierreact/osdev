# VPATH
#http://www.gnu.org/software/make/manual/make.html#Automatic-Variables

CC=gcc
AS=nasm
LD=ld

SOURCES=    kmain.o \
            system.ports.o \
            system.monitor.o \
            system.keyboard.o \
            system.mem.o \
            system.shell.o \
            system.ide.o \
            system.fat32.o \
            system.acpi.o \
            system.apic.o \
            system.cpu.o

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
            -Wno-packed-bitfield-compat \
            -O \
            -fstrength-reduce \
            -fomit-frame-pointer \
            -finline-functions \
            -fno-PIC \
    		-nostdinc

LDFLAGS=    -nostdlib \
            -Tlink.ld

ASFLAGS=    -felf64

#vpath %.h ../include
#vpath %.c .
#vpath %.s .


all: ap_trampoline.bin $(SOURCES) bootsector.bin link

clean:
	-rm *.o kernel bootsector.bin ap_trampoline.bin

link:
	$(LD) $(LDFLAGS) -o kernel $(SOURCES)

ap_trampoline.bin: ap_trampoline.asm
	nasm -f bin -o ap_trampoline.bin ap_trampoline.asm

bootsector.bin: bootsector.asm
	nasm -f bin -o bootsector.bin bootsector.asm

.s.o:
	$(AS) $(ASFLAGS) $<




