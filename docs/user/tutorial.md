# Writing Your First Isurus Application

This tutorial walks through building and running a user application on Isurus. The demo app in `apps/demo_app/` is the starting point.

## Prerequisites

Install the cross-compilation tools:

```bash
cd apps
devbox shell
```

Or manually: `gcc`, `ld` (binutils), `make`.

## Directory structure

```
apps/
  link.ld               # Userland linker script (binary at 0x400000)
  Makefile              # Top-level: builds libc, then all apps
  libc/                 # Minimal C library wrapping Isurus syscalls
    types.h             # uint8, uint16, uint32, uint64, size_t
    syscall.h           # Raw SYSCALL instruction wrappers
    isurus.h            # ThreadMeta struct, exit(), yield()
    stdio.h / stdio.c   # putc, puts, print_dec, print_hex
    string.h / string.c # memcpy, memset, strlen
    Makefile            # Builds libc.a
  demo_app/
    main.c              # Application source
    Makefile            # Links against libc.a, outputs flat binary
```

## Build

From the repo root:

```bash
scripts/compile_qemu.sh
```

This builds the kernel, then `apps/`, then packs the ISO. The demo app binary ends up at `/bin/demo_app` on the ISO.

To build apps only:

```bash
make -C apps
```

## Writing an application

### Entry point

The entry point is `_start()` (not `main`). The linker script expects this symbol.

```c
#include "../libc/isurus.h"
#include "../libc/stdio.h"

void _start(void) {
    puts("Hello from ring 3\n");
    exit();
}
```

### Available functions

See [libc-reference.md](libc-reference.md) for the full API.

Key functions:
- `puts(s)` — print a string
- `putc(c)` — print a character
- `print_dec(n)` — print a decimal number
- `print_hex8(n)` / `print_hex16(n)` — print hex
- `exit()` — terminate the thread
- `yield()` — yield to next BSP task (no-op on APs)

### ThreadMeta

Each thread has access to a `ThreadMeta` struct describing its CPU, NUMA node, and assigned NIC. When AP ring 3 execution is implemented, this will be at a fixed virtual address. The struct layout is in `libc/isurus.h`.

```c
typedef struct {
    uint32 cpu_index;
    uint32 numa_node;
    uint32 nic_index;
    uint16 nic_segment;
    uint8  nic_bus;
    uint8  nic_dev;
    uint8  nic_func;
    uint8  reserved[3];
    uint8  nic_mac[6];
    uint8  reserved2[2];
} ThreadMeta;
```

### Syscalls

Applications use the SYSCALL instruction to request kernel services. The `libc/syscall.h` header provides inline wrappers. See [syscall-reference.md](syscall-reference.md) for the complete table.

### Linking

Apps are linked as flat binaries (not ELF) via `apps/link.ld`. The load address is `0x400000`. The kernel's binary loader reads the flat binary from the ISO and maps it at that address before dropping to ring 3.

### Adding a new app

1. Create `apps/my_app/main.c` and `apps/my_app/Makefile` (copy from `demo_app`)
2. Add `my_app` to `apps/Makefile`
3. Add `cp apps/my_app/my_app "$ISO_ROOT/bin/my_app"` to `scripts/compile.sh`
