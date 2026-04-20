// Miscellaneous shell commands: help, clear, echo, ring level, reboot,
// SMP test, PCI enumeration, and app-launcher commands.

#include "shell_internal.h"
#include <arch/ports.h>
#include <arch/acpi.h>
#include <drivers/pci.h>
#include <kernel/app.h>
#include <syscall.h>

void cmd_help(void) {
    sh_print("Available commands:\n");
    sh_print("  help            - Show this help message\n");
    sh_print("  clear           - Clear the screen\n");
    sh_print("  echo            - Echo text to screen\n");
    sh_print("  sys.reboot      - Reboot the system\n");
    sh_print("  sys.cpu.ls      - Show CPU and NUMA topology\n");
    sh_print("  sys.cpu.ring    - Show current privilege level\n");
    sh_print("  sys.mem.info    - Display memory information\n");
    sh_print("  sys.mem.free    - Show memory usage\n");
    sh_print("  sys.mem.test    - Test heap allocator\n");
    sh_print("  sys.acpi.ls     - List ACPI tables\n");
    sh_print("  sys.disk.ls     - List block devices\n");
    sh_print("  sys.disk.info   - Show disk information\n");
    sh_print("  sys.disk.read   - Read disk sectors\n");
    sh_print("  sys.disk.write  - Write disk sectors\n");
    sh_print("  sys.fs.ls [p]   - List directory (/ = ISO root, /mnt/* = FAT32)\n");
    sh_print("  sys.fs.cat <p>  - Print file contents\n");
    sh_print("  sys.pci.ls      - List PCI devices\n");
    sh_print("  sys.nic.ls      - List network interfaces\n");
    sh_print("  sys.nic.mode    - Show or set NIC assignment mode (per-numa|per-core)\n");
    sh_print("  sys.thread.ls   - Show per-CPU thread metadata (NUMA, NIC)\n");
    sh_print("  sys.net.arp     - Show ARP table (mgmt NIC)\n");
    sh_print("  sys.net.arping  - Send ARP request for IP address\n");
    sh_print("  sys.net.stats   - Show L2 frame counters\n");
    sh_print("  sys.net.trace   - Show packet trace log\n");
}

void cmd_clear(void) {
    sh_cls();
}

void cmd_echo(void) {
    char *text = cmd_buffer + 4;
    while (*text == ' ') text++;
    if (*text) {
        sh_print(text);
        sh_putc('\n');
    }
}

void cmd_ring(void) {
    uint16 cs;
    __asm__ volatile("mov %%cs, %0" : "=r"(cs));
    uint8 cpl = cs & 0x3;
    sh_print("Current privilege level: ring ");
    sh_print_dec(cpl);
    sh_print("\n");
    sh_print("CS: ");
    sh_print_hex(cs, "\n");
}

// cmd_reboot runs in ring 0 via SYS_REBOOT syscall handler
void cmd_reboot(void) {
    sh_print("Rebooting system...\n");
    __asm__ __volatile__("cli");

    ACPIResetRegInfo reset;
    if (acpi_reset_reg_info(&reset)) {
        if (reset.space_id == 1 && reset.address <= 0xFFFF) {
            uint16 port = (uint16)reset.address;
            if (reset.access_size == 2)
                outw(port, (uint16)reset.value);
            else
                outb(port, reset.value);
        } else if (reset.space_id == 0 && reset.address != 0) {
            volatile uint8 *mmio = (volatile uint8 *)(uint64)reset.address;
            *mmio = reset.value;
        }
        for (volatile int i = 0; i < 100000; i++);
    }

    outb(0xCF9, 0x00);
    outb(0xCF9, 0x02);
    outb(0xCF9, 0x06);
    for (volatile int i = 0; i < 100000; i++);

    uint8 temp;
    int timeout = 10000;
    do {
        temp = inb(0x64);
        if (--timeout == 0) break;
    } while (temp & 0x02);
    outb(0x64, 0xFE);
    for (volatile int i = 0; i < 100000; i++);

    struct {
        uint16 limit;
        uint64 base;
    } __attribute__((packed)) idtr = { 0, 0 };
    __asm__ __volatile__("lidt %0" : : "m"(idtr));
    __asm__ __volatile__("int $0x03");

    while (1) { __asm__ __volatile__("hlt"); }
}

void cmd_reboot_wrap(void) {
    if (use_syscalls) sys_reboot();
    else cmd_reboot();
}

void cmd_test_ap_wrap(void) {
    if (use_syscalls) sys_test_ap();
}

void cmd_lspci(void) {
    uint32 count = pci_get_device_count();
    if (count == 0) {
        sh_print("No PCI devices found\n");
        return;
    }
    for (uint32 i = 0; i < count; i++) {
        const PCIDevice *d = pci_get_device(i);
        sh_print_hex8(d->bus);
        sh_putc(':');
        sh_print_hex8(d->dev);
        sh_putc('.');
        sh_print_dec(d->func);
        sh_print("  ");
        sh_print_hex16(d->vendor_id);
        sh_putc(':');
        sh_print_hex16(d->device_id);
        sh_print("  class ");
        sh_print_hex8(d->class_code);
        sh_putc(':');
        sh_print_hex8(d->subclass);
        sh_print("  NUMA ");
        if (d->numa_node == PCI_NUMA_UNKNOWN)
            sh_putc('-');
        else
            sh_print_dec(d->numa_node);
        const char *vname = pci_vendor_name(d->vendor_id);
        const char *cname = pci_class_name(d->class_code, d->subclass);
        sh_print("  ");
        sh_print(vname ? (char *)vname : "?");
        sh_print("  ");
        sh_print(cname ? (char *)cname : "?");
        sh_putc('\n');
    }
}

void cmd_proc_run(void) {
    char *path = cmd_buffer + 13;  // skip "sys.proc.run "
    while (*path == ' ') path++;
    if (*path) {
        if (use_syscalls) sys_syscall1(SYS_APP_LAUNCH, (long)path);
    } else {
        sh_print("Usage: sys.proc.run <manifest_path>\n");
    }
}

void cmd_proc_ls(void) {
    if (use_syscalls) sys_syscall0(SYS_APP_LIST);
}
