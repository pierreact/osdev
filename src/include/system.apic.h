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

#endif
