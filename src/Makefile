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
            arch/acpi_madt.o \
            arch/acpi_numa.o \
            arch/acpi_platform.o \
            arch/acpi_lsacpi.o \
            arch/aml.o \
            kernel/mem.o \
            kernel/cpu.o \
            kernel/task.o \
            kernel/tss.o \
            kernel/syscall.o \
            kernel/stack_protector.o \
            kernel/bootmedia.o \
            kernel/loader.o \
            kernel/ini.o \
            kernel/app.o \
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
            net/eth.o \
            net/arp.o \
            net/pktbuf.o \
            net/l2.o \
            net/ip.o \
            net/icmp.o \
            net/pktrace.o \
            net/pktrace_kern.o \
            net/l2_kern.o \
            fs/fat32.o \
            fs/iso9660.o \
            fs/vfs.o \
            shell/shell.o \
            shell/shell_io.o \
            shell/shell_util.o \
            shell/shell_sys.o \
            shell/shell_mem.o \
            shell/shell_cpu.o \
            shell/shell_disk.o \
            shell/shell_fs.o \
            shell/shell_net.o

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
            -fno-stack-protector \
            -fzero-call-used-regs=skip \
            -fcf-protection=none \
            -fno-asynchronous-unwind-tables \
            -fno-PIC \
            -nostdinc

LDFLAGS=    -nostdlib \
            -Tlink.ld

ASFLAGS=    -felf64 -I boot/


all: ap_trampoline.bin $(OBJECTS) kernel.bin bootsector.bin ../apps/libc/libc.a

# libc.a is built as part of the kernel phase so the shared net sources
# in src/net/ (compiled into both kernel.bin and libc.a) stay in sync.
# Apps (demo_app, dpdk_l2) are built in a separate phase (apps/Makefile)
# and consume this artifact.
../apps/libc/libc.a: $(OBJECTS)
	$(MAKE) -C ../apps/libc

clean:
	-find . -name '*.o' -delete
	-rm -f kernel.bin bootsector.bin ap_trampoline.bin
	$(MAKE) -C ../apps/libc clean

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
