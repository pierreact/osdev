#ifndef SYSTEM_APIC_H
#define SYSTEM_APIC_H

#include <types.h>

// LAPIC register offsets (byte offsets from LAPIC base, MMIO)
#define LAPIC_ID        0x020   // Local APIC ID
#define LAPIC_VER       0x030   // Version
#define LAPIC_TPR       0x080   // Task Priority Register
#define LAPIC_EOI       0x0B0   // End of Interrupt
#define LAPIC_SVR       0x0F0   // Spurious Interrupt Vector Register
#define LAPIC_ESR       0x280   // Error Status Register
#define LAPIC_ICR_LO    0x300   // Interrupt Command Register (low 32 bits)
#define LAPIC_ICR_HI    0x310   // Interrupt Command Register (high 32 bits: destination)

// ICR delivery modes for INIT-SIPI-SIPI sequence
#define ICR_INIT        0x00000500
#define ICR_STARTUP     0x00000600
#define ICR_LEVEL_ASSERT 0x00004000
#define ICR_LEVEL_DEASSERT 0x00000000

// SVR bits
#define LAPIC_SVR_ENABLE 0x100

void disable_pic(void);
void lapic_init(void);
void ioapic_init(void);
void ap_startup(void);
void lapic_write(uint32 reg, uint32 val);
uint32 lapic_read(uint32 reg);
void ioapic_route_irq(uint8 gsi, uint8 vector, uint8 dest_lapic_id, uint16 flags);

// IDT vector for virtio-net INTx. Free range [0x30, 0xFE].
#define IRQ_VIRTIO_NET  0x40

// Read the BSP's LAPIC ID (the cpu_lapic_ids[0] from arch/acpi.c is
// also fine; this exposes a getter for net stack callers that don't
// pull in the full kernel/cpu.h).
uint8 lapic_bsp_id(void);

// Reprogram the 8254 PIT channel 0 to fire IRQ0 at the requested
// frequency in Hz. Bounds the input to [50, 10000]; outside that
// range the call is a no-op so a typo cannot brick the system or
// produce a divisor of zero. Real-hardware compatibility: PIT is
// PC-AT-legacy and present on every x86 system we'd target. On
// boards where PIT is disabled by firmware, the writes are silently
// ignored and the kernel keeps its default wake rate.
void pit_init_periodic(uint32 hz);

#endif
