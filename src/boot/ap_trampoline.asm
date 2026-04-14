; AP Trampoline - copied to TRAMPOLINE_BASE at runtime
; APs start here in 16-bit real mode after SIPI
;
; 16-bit code uses segment-relative addressing (DS=CS from SIPI).
; Protected/long mode code uses absolute linear addresses (TRAMPOLINE_BASE + offset).
;
; Data area at end of page (TRAMPOLINE_BASE + 0xFF0):
;   +0xFF0: CR3 value (uint64, patched by BSP)
;   +0xFF8: percpu base address (uint64, patched by BSP)

%define TRAMPOLINE_BASE 0x9F000
%define TRAMPOLINE_VECTOR (TRAMPOLINE_BASE >> 12)
%define PERCPU_SIZE 40

[BITS 16]
[ORG 0]                                                 ; Offsets are segment-relative; SIPI sets CS = TRAMPOLINE_BASE >> 4

ap_trampoline_entry:
    cli
    cld

    ; Set segments to match CS (SIPI set CS = TRAMPOLINE_BASE >> 4)
    mov ax, cs                                          ; CS = 0x9F00 from SIPI vector
    mov ds, ax                                          ; DS = CS for data access
    mov es, ax                                          ;
    mov ss, ax                                          ;
    mov sp, 0x0FF0                                      ; Temp stack near top of trampoline page

    ; Load the GDT for protected mode transition
    lgdt [ap_gdt_ptr]                                   ; DS-relative: reads from correct address

    ; Enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp dword 0x08:(TRAMPOLINE_BASE + ap_protected_mode) ; Absolute linear address for pmode

[BITS 32]
ap_protected_mode:
    mov ax, 0x10                                        ; Flat data segment
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; Enable PAE
    mov eax, cr4
    bts eax, 5                                          ; PAE
    mov cr4, eax

    ; Load CR3 with PML4T address (patched by BSP)
    mov eax, [TRAMPOLINE_BASE + 0xFF0]
    mov cr3, eax

    ; Enable Long Mode via EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    bts eax, 8                                          ; LME
    wrmsr

    ; Enable paging
    mov eax, cr0
    bts eax, 31                                         ; PG
    mov cr0, eax

    ; Jump to 64-bit mode
    jmp 0x18:(TRAMPOLINE_BASE + ap_long_mode)           ; 0x18 = 64-bit code segment in GDT

[BITS 64]
ap_long_mode:
    ; Set data segments
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Get our LAPIC ID to find our CPU index
    mov rdi, 0xFEE00000                                 ; LAPIC base
    mov eax, [rdi + 0x20]                               ; LAPIC ID register
    shr eax, 24                                         ; ID is in bits 31:24

    ; Find our index in percpu array
    mov rbx, [TRAMPOLINE_BASE + 0xFF8]                  ; percpu base (patched by BSP)
    mov rsi, rbx
    xor r12d, r12d                                      ; r12 = cpu index counter

.find_cpu:
    cmp r12d, 16                                        ; MAX_CPUS
    jge .halt                                           ; Not found, halt

    movzx edx, byte [rsi]                               ; percpu[i].lapic_id
    cmp dl, al
    je .found

    add rsi, PERCPU_SIZE                                ; Next entry (sizeof PerCPU)
    inc r12d
    jmp .find_cpu

.found:
    ; r12 = cpu index, rsi = &percpu[cpu_index]

    ; Load our stack from percpu[i].stack_top (offset 8)
    mov rsp, [rsi + 8]

    ; Enable LAPIC on this AP
    mov dword [rdi + 0x80], 0                           ; Clear TPR
    mov eax, [rdi + 0xF0]                               ; Read SVR
    or eax, 0x1FF                                       ; Enable + spurious vector 0xFF
    mov dword [rdi + 0xF0], eax                         ; Write SVR

    ; Compute pointer to our APWork entry: ap_work[cpu_index]
    ; ap_work base is patched by BSP at TRAMPOLINE_BASE + 0xFE8
    mov r13, [TRAMPOLINE_BASE + 0xFE8]                  ; ap_work base
    imul r14, r12, 32                                   ; APWORK_SIZE = 32
    add r13, r14                                        ; r13 = &ap_work[cpu_index]

    ; Signal that we're online: percpu[i].running = 1 (offset 1)
    mov byte [rsi + 1], 1

    ; Work-polling loop: check ap_work[cpu_index].ready,
    ; call fn(arg) when set, signal done, repeat.
    cli
.park:
    pause                                               ; spin-wait hint
    movzx eax, byte [r13]                               ; ap_work.ready (offset 0)
    test al, al
    jz .park                                            ; not ready, keep polling

    ; Work ready: call fn(arg)
    mov rdi, [r13 + 16]                                 ; ap_work.arg (offset 16)
    call [r13 + 8]                                      ; ap_work.fn (offset 8)
    mov [r13 + 24], rax                                 ; ap_work.result (offset 24)

    ; Signal completion and clear ready
    mov byte [r13 + 1], 1                               ; ap_work.done = 1
    mov byte [r13], 0                                   ; ap_work.ready = 0

    jmp .park

.halt:
    cli
    hlt
    jmp .halt

; ----- GDT for AP transition -----
align 16
ap_gdt:
    dw 0,0,0,0                                         ; Null descriptor
    dw 0xFFFF,0,0x9A00,0x00CF                           ; 32-bit code (0x08)
    dw 0xFFFF,0,0x9200,0x008F                           ; Flat data    (0x10)
    dw 0xFFFF,0,0x9A00,0x00AF                           ; 64-bit code (0x18)
ap_gdt_end:

ap_gdt_ptr:
    dw ap_gdt_end - ap_gdt - 1                          ; GDT limit
    dd TRAMPOLINE_BASE + ap_gdt                         ; GDT base (absolute linear address)
