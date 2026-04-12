# Isurus Syscall Reference

System calls are the interface between ring 3 user code and the ring 0 kernel. Invoked via the x86-64 `SYSCALL` instruction.

## Convention

| Register | Purpose |
|----------|---------|
| RAX | Syscall number |
| RDI | Argument 1 |
| RSI | Argument 2 |
| RDX | Argument 3 |
| R10 | Argument 4 |
| R8 | Argument 5 |
| R9 | Argument 6 |
| RAX (return) | Return value |

RCX and R11 are clobbered by SYSCALL (hardware saves RIP to RCX, RFLAGS to R11).

## Syscall Table

| Nr | Name | Arguments | Return | Description |
|----|------|-----------|--------|-------------|
| 0 | SYS_PUTC | RDI=char | - | Write one character to console |
| 1 | SYS_KPRINT | RDI=string_ptr | - | Print null-terminated string |
| 2 | SYS_CLS | - | - | Clear screen |
| 3 | SYS_HEAP_STATS | RDI=*used, RSI=*free, RDX=*total | - | Get heap block counts |
| 4 | SYS_KMALLOC | RDI=size | ptr or 0 | Allocate kernel memory (4KB granularity) |
| 5 | SYS_KFREE | RDI=ptr | - | Free kernel memory |
| 6 | SYS_IDE_READ | RDI=lba, RSI=count, RDX=buf | 0 or -1 | Read IDE sectors |
| 7 | SYS_IDE_WRITE | RDI=lba, RSI=buf | 0 or -1 | Write IDE sector |
| 8 | SYS_IDE_MODEL | - | string_ptr | Get IDE drive model string |
| 9 | SYS_IDE_SECTORS | - | sector_count | Get IDE drive total sectors |
| 10 | SYS_FAT32_LS | - | - | List FAT32 root directory |
| 11 | SYS_FAT32_CAT | RDI=filename_ptr | 0 or -1 | Print FAT32 file contents |
| 12 | SYS_ACPI_LS | - | - | List ACPI tables |
| 13 | SYS_REBOOT | - | (no return) | Reboot the system |
| 14 | SYS_CPU_INFO | RDI=buf_ptr | 0 | Fill SysCpuInfo struct |
| 15 | SYS_MEMINFO | RDI=buf_ptr | 0 | Fill SysMemInfo struct |
| 16 | SYS_KPRINT_DEC | RDI=number | - | Print decimal number |
| 17 | SYS_KPRINT_HEX | RDI=number, RSI=suffix_str | - | Print hex number + suffix |
| 18 | SYS_KPRINT_DECPAD | RDI=number, RSI=width | - | Print right-padded decimal |
| 19 | SYS_READCHAR | - | char or 0 | Read one character (non-blocking) |
| 20 | SYS_WAIT_INPUT | - | char | Wait for keyboard/serial input (blocking) |
| 21 | SYS_YIELD | - | - | Yield to next BSP task |
| 22 | SYS_TASK_EXIT | - | (no return) | Exit current task/thread |
| 23 | SYS_ISO_LS | RDI=path_ptr | - | List VFS directory |
| 24 | SYS_ISO_READ | RDI=path_ptr, RSI=buf, RDX=max | bytes_read or -1 | Read file from VFS |
