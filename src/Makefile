# VPATH
#http://www.gnu.org/software/make/manual/make.html#Automatic-Variables

CC=gcc
AS=nasm
LD=ld

# Bootsector only: 1 = COM1 trace in bootsector.asm, 0 = strip (nasm -DBOOT_DEBUG=0)
BOOT_DEBUG ?= 1

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
            system.cpu.o \
            system.serial.o \
            system.tss.o \
            system.syscall.o \
            system.task.o \
            system.stack_protector.o \
            system.pci.o \
            system.virtio.o \
            system.virtio_net.o \
            system.nic.o

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
            -fstack-protector-strong \
            -mstack-protector-guard=global \
            -fno-PIC \
    		-nostdinc

LDFLAGS=    -nostdlib \
            -Tlink.ld

ASFLAGS=    -felf64

#vpath %.h ../include
#vpath %.c .
#vpath %.s .


all: ap_trampoline.bin $(SOURCES) kernel bootsector.bin

clean:
	-rm *.o kernel bootsector.bin ap_trampoline.bin

# Linked kernel binary (size drives bootsector KSIZE).
kernel: $(SOURCES)
	$(LD) $(LDFLAGS) -o kernel $(SOURCES)

link: kernel
	@true

# KSIZE = ceil(kernel_bytes/512) + margin so INT 13h loads the full kernel (never partial).
bootsector.bin: bootsector.asm kernel
	@KB=$$(wc -c < kernel); \
	SECS=$$(( ($$KB + 511) / 512 )); \
	MARGIN=32; \
	KSIZE=$$(($$SECS + $$MARGIN)); \
	MAX=$$(($$KSIZE * 512)); \
	echo "bootsector: kernel $$KB bytes ($$SECS sectors) => KSIZE=$$KSIZE sectors (max $$MAX bytes)"; \
	test $$KB -le $$MAX || (echo "error: kernel ($$KB bytes) exceeds KSIZE load area ($$MAX bytes)" >&2; exit 1); \
	$(AS) -f bin -o bootsector.bin -DKSIZE=$$KSIZE -DBOOT_DEBUG=$(BOOT_DEBUG) bootsector.asm

ap_trampoline.bin: ap_trampoline.asm
	nasm -f bin -o ap_trampoline.bin ap_trampoline.asm

.s.o:
	$(AS) $(ASFLAGS) $<




