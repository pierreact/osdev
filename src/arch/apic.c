#include <types.h>
#include <arch/apic.h>
#include <arch/acpi.h>
#include <kernel/cpu.h>
#include <arch/ports.h>
#include <drivers/monitor.h>

// MMIO base pointers, set during init
static volatile uint32 *lapic_ptr = NULL;
static volatile uint32 *ioapic_ptr = NULL;

// LAPIC registers are 32-bit, spaced 16 bytes apart (reg offset / 4 = array index)
void lapic_write(uint32 reg, uint32 val) {
    lapic_ptr[reg / 4] = val;
}

uint32 lapic_read(uint32 reg) {
    return lapic_ptr[reg / 4];
}

// I/O APIC uses indirect register access: write index to IOREGSEL, read/write IOWIN
static void ioapic_write(uint32 reg, uint32 val) {
    ioapic_ptr[0] = reg;       // IOREGSEL
    ioapic_ptr[4] = val;       // IOWIN (offset 0x10 / 4 = 4)
}

static uint32 ioapic_read(uint32 reg) {
    ioapic_ptr[0] = reg;
    return ioapic_ptr[4];
}

void disable_pic(void) {
    // Mask all PIC IRQs
    outb(0xA1, 0xFF);
    outb(0x21, 0xFF);
    kprint("PIC: Disabled (all IRQs masked)\n");
}

// Enable Local APIC on BSP: clear task priority, set spurious vector
void lapic_init(void) {
    lapic_ptr = (volatile uint32 *)(uint64)lapic_base_addr;

    // Clear Task Priority Register - accept all interrupts
    lapic_write(LAPIC_TPR, 0);

    // Clear Error Status Register (write then read)
    lapic_write(LAPIC_ESR, 0);
    lapic_read(LAPIC_ESR);

    // Enable LAPIC via Spurious Vector Register
    // Set spurious vector to 0xFF and enable bit
    lapic_write(LAPIC_SVR, LAPIC_SVR_ENABLE | 0xFF);

    kprint("LAPIC: Enabled on BSP (ID ");
    kprint_dec(lapic_read(LAPIC_ID) >> 24);
    kprint(")\n");
}

// Write a redirection table entry: map a GSI to a vector, routed to dest LAPIC.
// Polarity and trigger mode come from MADT Interrupt Source Override flags.
static void ioapic_route_irq(uint8 gsi, uint8 vector, uint8 dest_lapic_id, uint16 flags) {
    uint32 redtbl_reg = 0x10 + gsi * 2; // Each entry is 2 registers (lo + hi)

    // Low 32 bits: vector, delivery mode, polarity, trigger mode
    uint32 lo = vector;

    // Check polarity from ISO flags (bits 1:0)
    uint8 polarity = flags & 0x3;
    if (polarity == 3) {
        lo |= (1 << 13); // Active low
    }

    // Check trigger mode from ISO flags (bits 3:2)
    uint8 trigger = (flags >> 2) & 0x3;
    if (trigger == 3) {
        lo |= (1 << 15); // Level triggered
    }

    // High 32 bits: destination LAPIC ID
    uint32 hi = ((uint32)dest_lapic_id) << 24;

    ioapic_write(redtbl_reg + 1, hi);
    ioapic_write(redtbl_reg, lo);
}

// Init I/O APIC: mask everything, then route timer (IRQ0) and keyboard (IRQ1) to BSP
void ioapic_init(void) {
    ioapic_ptr = (volatile uint32 *)(uint64)ioapic_base_addr;

    // Read max redirection entries from version register
    uint32 ver = ioapic_read(0x01);
    uint32 max_redir = ((ver >> 16) & 0xFF);

    // Mask all redirection entries first
    for (uint32 i = 0; i <= max_redir; i++) {
        uint32 reg = 0x10 + i * 2;
        ioapic_write(reg, 0x00010000); // Masked
        ioapic_write(reg + 1, 0);
    }

    uint8 bsp_lapic_id = cpu_lapic_ids[0];

    // Route timer: check if MADT has an ISO remapping IRQ0 to a different GSI
    uint32 timer_gsi = 0;
    uint16 timer_flags = 0;
    for (uint32 i = 0; i < iso_count; i++) {
        if (iso_entries[i].irq_source == 0) {
            timer_gsi = iso_entries[i].gsi;
            timer_flags = iso_entries[i].flags;
            break;
        }
    }
    if (timer_gsi == 0) timer_gsi = 0; // Default: GSI 0 if no ISO
    // Route timer to vector 0x20
    ioapic_route_irq(timer_gsi, 0x20, bsp_lapic_id, timer_flags);

    // Route keyboard: check for ISO remapping IRQ1
    uint32 kb_gsi = 1;
    uint16 kb_flags = 0;
    for (uint32 i = 0; i < iso_count; i++) {
        if (iso_entries[i].irq_source == 1) {
            kb_gsi = iso_entries[i].gsi;
            kb_flags = iso_entries[i].flags;
            break;
        }
    }
    ioapic_route_irq(kb_gsi, 0x21, bsp_lapic_id, kb_flags);

    // Route COM1: check for ISO remapping IRQ4
    uint32 com1_gsi = 4;
    uint16 com1_flags = 0;
    for (uint32 i = 0; i < iso_count; i++) {
        if (iso_entries[i].irq_source == 4) {
            com1_gsi = iso_entries[i].gsi;
            com1_flags = iso_entries[i].flags;
            break;
        }
    }
    ioapic_route_irq(com1_gsi, 0x24, bsp_lapic_id, com1_flags);

    kprint("IOAPIC: Timer->vec 0x20 (GSI ");
    kprint_dec(timer_gsi);
    kprint("), Keyboard->vec 0x21 (GSI ");
    kprint_dec(kb_gsi);
    kprint("), COM1->vec 0x24 (GSI ");
    kprint_dec(com1_gsi);
    kprint(")\n");
}

static void delay_us(uint32 us) {
    // Use PIT channel 2 for short delays
    // Each PIT tick is ~838ns, so us * 1000 / 838 ticks
    // Simple busy loop approximation: read port 0x80 takes ~1us
    for (uint32 i = 0; i < us; i++) {
        inb(0x80);
    }
}

// Wake APs via INIT-SIPI-SIPI sequence. Trampoline at TRAMPOLINE_BASE brings each AP
// from real mode to long mode, then it sets its percpu.running flag.
void ap_startup(void) {
    if (cpu_count <= 1) {
        kprint("SMP: Only 1 CPU, no APs to wake\n");
        return;
    }

    // Copy trampoline below kernel (IVT page, no longer needed in long mode)
    #define TRAMPOLINE_BASE  0x9F000
    #define TRAMPOLINE_VECTOR (TRAMPOLINE_BASE >> 12)

    extern uint8 ap_trampoline_start[];
    extern uint8 ap_trampoline_end[];
    uint64 trampoline_size = (uint64)ap_trampoline_end - (uint64)ap_trampoline_start;

    uint8 *dest = (uint8 *)TRAMPOLINE_BASE;
    uint8 *src = ap_trampoline_start;
    for (uint64 i = 0; i < trampoline_size; i++) {
        dest[i] = src[i];
    }

    // Patch data area at end of trampoline page.
    // The AP reads CR3 and percpu base from here during startup.
    extern uint32 PML4T_LOCATION;
    volatile uint64 *patch_cr3 = (volatile uint64 *)(TRAMPOLINE_BASE + 0xFF0);
    volatile uint64 *patch_percpu = (volatile uint64 *)(TRAMPOLINE_BASE + 0xFF8);

    *patch_cr3 = (uint64)PML4T_LOCATION;
    *patch_percpu = (uint64)&percpu[0];

    // Patch ap_work base for AP polling loop
    volatile uint64 *patch_apwork = (volatile uint64 *)(TRAMPOLINE_BASE + 0xFE8);
    *patch_apwork = (uint64)&ap_work[0];

    // Send INIT-SIPI-SIPI to each AP
    for (uint32 i = 1; i < cpu_count; i++) {
        uint8 apic_id = cpu_lapic_ids[i];

        // Send INIT IPI
        lapic_write(LAPIC_ICR_HI, ((uint32)apic_id) << 24);
        lapic_write(LAPIC_ICR_LO, ICR_INIT | ICR_LEVEL_ASSERT);

        delay_us(10000); // 10ms

        // Deassert INIT
        lapic_write(LAPIC_ICR_HI, ((uint32)apic_id) << 24);
        lapic_write(LAPIC_ICR_LO, ICR_INIT | ICR_LEVEL_DEASSERT);

        delay_us(200);

        // Send SIPI #1
        lapic_write(LAPIC_ICR_HI, ((uint32)apic_id) << 24);
        lapic_write(LAPIC_ICR_LO, ICR_STARTUP | TRAMPOLINE_VECTOR);

        delay_us(200);

        // Send SIPI #2
        lapic_write(LAPIC_ICR_HI, ((uint32)apic_id) << 24);
        lapic_write(LAPIC_ICR_LO, ICR_STARTUP | TRAMPOLINE_VECTOR);

        delay_us(200);

        // Wait for AP to check in (with timeout)
        uint32 timeout = 100000;
        while (!percpu[i].running && timeout > 0) {
            delay_us(10);
            timeout--;
        }

        if (percpu[i].running) {
            kprint("SMP: CPU ");
            kprint_dec(i);
            kprint(" (LAPIC ");
            kprint_dec(apic_id);
            kprint(") online\n");
        } else {
            kprint("SMP: CPU ");
            kprint_dec(i);
            kprint(" FAILED to start!\n");
        }
    }

    // Count online CPUs
    uint32 online = 0;
    for (uint32 i = 0; i < cpu_count; i++) {
        if (percpu[i].running) online++;
    }
    kprint("SMP: ");
    kprint_dec(online);
    kprint(" CPUs online\n");
}
