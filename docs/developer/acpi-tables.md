# ACPI Table Reference

## APIC (MADT) - Multiple APIC Description Table
Lists all CPUs (LAPIC IDs), I/O APICs, and interrupt source overrides.
This is how the kernel discovers how many CPUs exist and where the interrupt controllers are mapped.

## FACP (FADT) - Fixed ACPI Description Table
Power management and system control.
Contains the address of the reset register, PM timer address (for precise timing without PIT), and pointers to the FACS and DSDT.
Also tells you if the system supports ACPI sleep states (S1-S5).

## DSDT - Differentiated System Description Table
AML (ACPI Machine Language) bytecode describing the entire machine topology - devices, power states, thermal zones, GPE (General Purpose Event) handlers.
We'd need an AML interpreter to use it. Linux has one, it's ~50k lines.

## SSDT - Secondary System Description Table
Supplemental AML bytecode, same format as DSDT.
QEMU uses these to describe CPU hotplug, dynamic devices, etc.
Same situation, needs an AML interpreter.

## SRAT - System Resource Affinity Table
Maps CPUs and memory ranges to NUMA proximity domains.
Each entry says "CPU X belongs to NUMA node Y" and "memory range A-B belongs to NUMA node Z".

## SLIT - System Locality Information Table
NxN matrix of relative latency distances between NUMA nodes.
Entry [i][j] = relative cost of node i accessing node j's memory.
Typically 10 for local, 20-21 for remote socket.
The scheduler would use this to decide migration cost when respinning a thread post failure if original core is not eligible.
When you add cluster nodes, rNUMA applies and we model them with higher values (e.g. 100+).

## HPET - High Precision Event Timer
Hardware timer with nanosecond resolution, much better than the 8254 PIT (which is ~1.19 MHz).
Gives the MMIO base address and number of timer comparators.
Useful for precise scheduling, profiling, and replacing the PIT as system timer.

## MCFG - PCI Express Memory Mapped Configuration
Base address of the PCIe ECAM (Enhanced Configuration Access Mechanism) region.
Instead of using legacy I/O ports 0xCF8/0xCFC to configure PCI devices (limited to 256 bytes per function), ECAM gives you 4KB per function via MMIO.
Used to enumerate and configure NICs.

## DMAR - DMA Remapping Table (Intel VT-d)
Describes the IOMMU - hardware that restricts which memory regions a device can DMA to.
Critical for security (prevents a rogue NIC from writing anywhere in RAM) and for device passthrough in VMs.
Intel-specific, AMD equivalent is IVRS.

## IVRS - I/O Virtualization Reporting Structure (AMD-Vi)
AMD's equivalent of DMAR. Same purpose: IOMMU configuration for DMA protection and device isolation.

## WAET - Windows ACPI Emulated Devices Table
QEMU-specific hint telling the OS that the RTC and PM timer are "enlightened" (don't need workarounds for hardware bugs).

## HEST - Hardware Error Source Table
Describes hardware error reporting sources: machine check exceptions, PCIe AER (Advanced Error Reporting), firmware-first error handling.
For building reliable systems that detect and recover from hardware errors.

## BERT - Boot Error Record Table
Points to a memory region where firmware stored error records from the previous boot.
If the machine crashed due to hardware error, BERT tells what happened.

## EINJ - Error Injection Table
Lets you inject fake hardware errors for testing your error handling code.
Useful in testing though not something QEMU typically exposes.

## ERST - Error Record Serialization Table
Persistent storage for error records across reboots (typically in flash/NVRAM).
The OS writes error info before crashing so it survives reboot.

## BGRT - Boot Graphics Resource Table
Contains the boot splash screen image (the vendor logo).
Irrelevant for this project.

## DRTM - Dynamic Root of Trust for Measurement
Intel TXT / AMD SKINIT support for measured launch. Security feature for trusted boot chains.
Not relevant unless you're doing secure boot.

## TPM2 - Trusted Platform Module 2.0
Describes the TPM device interface. Used for secure key storage, attestation, measured boot.
Not relevant for this project.
