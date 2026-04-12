# Isurus Documentation

## Getting Started

- [Building and Running](getting-started/building.md) -- toolchain setup, build scripts, QEMU configuration
- [Hardware Requirements](getting-started/hardware.md) -- supported processors and DMA requirements

## For Researchers

- [Research Overview](research/overview.md) -- motivation, core concepts, NUMA model, DSM architecture, evaluation goals

## For Administrators

- [Deployment](admin/deployment.md) -- boot media, cluster setup, cloud deployment

## For Application Developers

- [Application Model](user/application-model.md) -- execution model, memory model, device access, thread placement, fault tolerance, comparison with Linux for HPC
- [Tutorial](user/tutorial.md) -- write, build, and run your first Isurus application
- [Libc Reference](user/libc-reference.md) -- userland C library API (stdio, string, isurus.h)
- [Syscall Reference](user/syscall-reference.md) -- complete syscall table with numbers and arguments

## For Kernel Developers

- [Architecture](developer/architecture.md) -- design decisions and rationale (boot, memory, execution, devices, DSM, coherence, fault tolerance)
- [Memory Map](developer/memory-map.md) -- physical memory layout
- [Debugging](developer/debugging.md) -- boot traces, GDB setup, QEMU monitor
- [Interrupts](developer/interrupts.md) -- x86-64 interrupt vector table
- [ACPI Tables](developer/acpi-tables.md) -- ACPI table reference (MADT, SRAT, SLIT, MCFG, DMAR, etc.)
- [Glossary](developer/glossary.md) -- terminology reference

## Project

- [Roadmap](project/todo.md) -- completed work and upcoming tasks
- [Changelog](../CHANGELOG.md) -- release history
