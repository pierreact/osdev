# OS Development - Debugging Guide

**Audience:** Kernel developers. For memory layout details, see `memory_map.md`. For architecture decisions, see `ARCHITECTURE.md`.

## Quick Start

### 1. Run Normally (with reboot enabled)
```bash
./compile_qemu.sh
```

The OS will boot in QEMU with debug logging enabled. The reboot command will work normally.

### 2. Run in Debug Mode (no reboot on crashes)
```bash
./compile_qemu_debug.sh
```

The OS will boot in QEMU with full debug logging:
- Interrupts logged
- CPU resets logged  
- Guest errors logged
- `-no-reboot` and `-no-shutdown` prevent automatic restarts on triple faults
- Use this when debugging crashes to examine the state

### 2. Check the Debug Log
After a crash or issue, check the QEMU log:
```bash
cat /tmp/qemu.log | tail -100
```

Or use the helper script:
```bash
./check_qemu_log.sh
```

### 3. Interactive Debugging with GDB

**Terminal 1** - Start QEMU:
```bash
./compile_qemu.sh
```

**Terminal 2** - Connect GDB:
```bash
./gdb_debug.sh
```

## What to Look For in QEMU Log

### Triple Fault
```
check_exception old: 0xffffffff new 0xe
Triple fault
```
This means the CPU hit an exception while handling another exception.

### CPU Reset
```
CPU Reset (CPU 0)
```
Usually caused by triple fault or invalid paging structures.

### Page Fault (Exception 14 / 0xe)
```
check_exception old: 0xffffffff new 0xe
```
This is exception 14 (page fault). Common causes:
- Invalid PML4/PDPT/PD/PT entry
- Missing PRESENT bit in paging structure
- Accessing unmapped memory

### General Protection Fault (Exception 13 / 0xd)
```
check_exception old: 0xffffffff new 0xd
```
Common causes:
- Invalid segment selector
- Privilege level violation
- Invalid descriptor

## Common Issues and Solutions

### Machine Resets When Enabling PAE
**Symptoms**: Reset right after "CR3 loaded" message
**Cause**: Invalid paging structures
**Check**:
1. PML4T address is valid (should be 0x100000)
2. First PML4E has PRESENT bit set
3. All referenced tables exist and are valid

### Machine Resets When Enabling Paging
**Symptoms**: Reset after "PAE Enabled" message
**Cause**: Paging structures don't map currently executing code
**Solution**: Ensure low memory (0x0 - 0x200000) is identity mapped

## GDB Commands

```gdb
# Connect to QEMU
target remote localhost:1234

# Show registers
info registers

# Examine memory at address
x/10x 0x100000    # Show 10 hex values at PML4T

# Disassemble
x/10i $pc         # Show next 10 instructions

# Step one instruction
si

# Continue execution
c

# Set breakpoint
b *0x1000         # Break at address
```

## QEMU Monitor Commands

In the QEMU monitor (stdio):
```
info registers     # Show CPU registers
info mem           # Show memory mappings (after paging enabled)
info tlb           # Show TLB entries
x /10x 0x100000    # Examine memory
```

## Memory Layout

See `memory_map.md` for the full memory map.
