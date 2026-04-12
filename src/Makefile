# Build configuration
CC=gcc
AS=nasm
LD=ld

# Bootsector only: 1 = COM1 trace in bootsector.asm, 0 = strip (nasm -DBOOT_DEBUG=0)
BOOT_DEBUG ?= 1

OBJECTS=    boot/kmain.o \
            arch/ports.o \
            arch/apic.o \
            arch/acpi.o \
            arch/aml.o \
            kernel/mem.o \
            kernel/cpu.o \
            kernel/task.o \
            kernel/tss.o \
            kernel/syscall.o \
            kernel/stack_protector.o \
            kernel/bootmedia.o \
            drivers/serial.o \
            drivers/monitor.o \
            drivers/keyboard.o \
            drivers/ide.o \
            drivers/pci.o \
            drivers/pci_ids.o \
            drivers/ahci.o \
            net/nic.o \
            net/virtio.o \
            net/virtio_net.o \
            fs/fat32.o \
            fs/iso9660.o \
            fs/vfs.o \
            shell/shell.o

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

ASFLAGS=    -felf64 -I boot/


all: ap_trampoline.bin $(OBJECTS) kernel.bin bootsector.bin

clean:
	-find . -name '*.o' -delete
	-rm -f kernel.bin bootsector.bin ap_trampoline.bin

# Linked kernel binary (size drives bootsector KSIZE).
kernel.bin: $(OBJECTS)
	$(LD) $(LDFLAGS) -o kernel.bin $(OBJECTS)

# C files to object files (subdirectories preserved)
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Assembly files (kmain.s lives in boot/)
boot/kmain.o: boot/kmain.s
	$(AS) $(ASFLAGS) -o $@ $<

# KSIZE = ceil(kernel_bytes/512) + margin so INT 13h loads the full kernel (never partial).
bootsector.bin: boot/bootsector.asm kernel.bin
	@KB=$$(wc -c < kernel.bin); \
	SECS=$$(( ($$KB + 511) / 512 )); \
	MARGIN=32; \
	KSIZE=$$(($$SECS + $$MARGIN)); \
	MAX=$$(($$KSIZE * 512)); \
	echo "bootsector: kernel $$KB bytes ($$SECS sectors) => KSIZE=$$KSIZE sectors (max $$MAX bytes)"; \
	test $$KB -le $$MAX || (echo "error: kernel ($$KB bytes) exceeds KSIZE load area ($$MAX bytes)" >&2; exit 1); \
	$(AS) -f bin -o bootsector.bin -I boot/ -DKSIZE=$$KSIZE -DBOOT_DEBUG=$(BOOT_DEBUG) boot/bootsector.asm

ap_trampoline.bin: boot/ap_trampoline.asm
	$(AS) -f bin -I boot/ -o ap_trampoline.bin boot/ap_trampoline.asm
