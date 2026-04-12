# Isurus Libc Reference

Minimal C library for Isurus userland applications. Located in `apps/libc/`. Linked statically via `libc.a`.

## Headers

| Header | Contents |
|--------|----------|
| `types.h` | Integer types, `size_t`, `NULL` |
| `syscall.h` | Raw `SYSCALL` wrappers: `syscall0`, `syscall1`, `syscall2` |
| `isurus.h` | `ThreadMeta` struct, `exit()`, `yield()`, constants |
| `stdio.h` | Console output functions |
| `string.h` | Memory and string utilities |

## types.h

| Type | Width | Equivalent |
|------|-------|-----------|
| `uint8` | 8-bit | `unsigned char` |
| `uint16` | 16-bit | `unsigned short` |
| `uint32` | 32-bit | `unsigned int` |
| `uint64` | 64-bit | `unsigned long` |
| `int8` / `int16` / `int32` | signed variants | |
| `size_t` | 64-bit | `unsigned long` |

## isurus.h

### ThreadMeta

Per-CPU metadata exposed by the kernel.

```c
typedef struct {
    uint32 cpu_index;       // CPU number (0 = BSP, 1+ = APs)
    uint32 numa_node;       // NUMA proximity domain
    uint32 nic_index;       // Assigned NIC slot, or NIC_NONE
    uint16 nic_segment;     // PCI segment of assigned NIC
    uint8  nic_bus;         // PCI bus
    uint8  nic_dev;         // PCI device
    uint8  nic_func;        // PCI function
    uint8  reserved[3];
    uint8  nic_mac[6];      // MAC address
    uint8  reserved2[2];
} ThreadMeta;
```

### Constants

| Name | Value | Meaning |
|------|-------|---------|
| `NIC_NONE` | `0xFFFFFFFF` | No NIC assigned to this CPU |
| `THREAD_NUMA_UNKNOWN` | `0xFFFFFFFF` | NUMA node not determined |

### Functions

| Signature | Description |
|-----------|------------|
| `void exit(void)` | Terminate the current thread. AP re-parks, BSP task exits. Does not return. |
| `void yield(void)` | Yield to next BSP cooperative task. No-op on APs. |

## stdio.h

All output goes through kernel syscalls (SYS_PUTC, SYS_KPRINT, SYS_KPRINT_DEC). Output appears on both VGA console and serial.

| Signature | Syscall | Description |
|-----------|---------|------------|
| `void putc(char c)` | SYS_PUTC | Print one character |
| `void puts(const char *s)` | SYS_KPRINT | Print null-terminated string |
| `void print_dec(uint64 n)` | SYS_KPRINT_DEC | Print decimal number |
| `void print_hex8(uint8 val)` | SYS_PUTC x2 | Print 2-digit hex (e.g., `"0a"`) |
| `void print_hex16(uint16 val)` | SYS_PUTC x4 | Print 4-digit hex (e.g., `"1af4"`) |

## string.h

| Signature | Description |
|-----------|------------|
| `void *memcpy(void *dest, const void *src, size_t n)` | Copy `n` bytes. Returns `dest`. |
| `void *memset(void *dest, int val, size_t n)` | Fill `n` bytes with `val`. Returns `dest`. |
| `size_t strlen(const char *s)` | String length (excluding null terminator). |

## syscall.h

Raw SYSCALL instruction wrappers. Use these only if libc doesn't provide a higher-level function.

| Signature | Description |
|-----------|------------|
| `long syscall0(long nr)` | Syscall with 0 args |
| `long syscall1(long nr, long a1)` | Syscall with 1 arg (RDI) |
| `long syscall2(long nr, long a1, long a2)` | Syscall with 2 args (RDI, RSI) |

Register convention: RAX = syscall number, RDI = arg1, RSI = arg2, RDX = arg3, R10 = arg4. Return value in RAX. RCX and R11 are clobbered by SYSCALL.

See [syscall-reference.md](syscall-reference.md) for the complete syscall table.
