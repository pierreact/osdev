; AP Trampoline - copied to 0x8000 at runtime
; APs start here in 16-bit real mode after SIPI
; SIPI vector 0x08 => start address 0x8000
;
; Data area at end of page (0x8FF0-0x8FFF):
;   0x8FF0: CR3 value (uint64)
;   0x8FF8: percpu base address (uint64)

[BITS 16]
[ORG 0x8000]

ap_trampoline_entry:
    cli
    cld

    ; Set segments
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7000          ; Temporary stack below trampoline

    ; Load the GDT for protected mode transition
    lgdt [ap_gdt_ptr]

    ; Enable protected mode
    mov eax, cr0
    or eax, 1
    mov cr0, eax

    jmp 0x08:ap_protected_mode

[BITS 32]
ap_protected_mode:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov fs, ax
    mov gs, ax

    ; Enable PAE
    mov eax, cr4
    bts eax, 5             ; PAE
    mov cr4, eax

    ; Load CR3 with PML4T address (patched by BSP)
    mov eax, [0x8FF0]
    mov cr3, eax

    ; Enable Long Mode via EFER MSR
    mov ecx, 0xC0000080
    rdmsr
    bts eax, 8             ; LME
    wrmsr

    ; Enable paging
    mov eax, cr0
    bts eax, 31            ; PG
    mov cr0, eax

    ; Jump to 64-bit mode
    jmp 0x18:ap_long_mode  ; 0x18 = 64-bit code segment in GDT

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
    mov rdi, 0xFEE00000        ; LAPIC base into register
    mov eax, [rdi + 0x20]      ; LAPIC ID register
    shr eax, 24                ; ID is in bits 31:24

    ; Find our index in percpu array
    ; percpu base is at 0x8FF8
    mov rbx, [0x8FF8]          ; percpu base
    mov rsi, rbx               ; rsi = current percpu entry pointer

.find_cpu:
    mov rcx, rsi
    sub rcx, rbx               ; rcx = byte offset from base
    cmp rcx, 16 * 16           ; MAX_CPUS * sizeof(PerCPU)
    jge .halt                  ; Not found, halt

    ; percpu[i].lapic_id is at offset 0
    movzx edx, byte [rsi]
    cmp dl, al
    je .found

    add rsi, 16                ; Next entry (sizeof PerCPU = 16)
    jmp .find_cpu

.found:
    ; Load our stack from percpu[i].stack_top (offset 8)
    mov rsp, [rsi + 8]

    ; Enable LAPIC on this AP (rdi still = 0xFEE00000)
    mov dword [rdi + 0x80], 0      ; Clear TPR
    mov eax, [rdi + 0xF0]          ; Read SVR
    or eax, 0x1FF                   ; Enable + spurious vector 0xFF
    mov dword [rdi + 0xF0], eax    ; Write SVR

    ; Signal that we're online: percpu[i].running = 1 (offset 1)
    mov byte [rsi + 1], 1

    ; Park: enable interrupts and halt loop
    sti
.park:
    hlt
    jmp .park

.halt:
    cli
    hlt
    jmp .halt

; ----- GDT for AP transition -----
align 16
ap_gdt:
    dw 0,0,0,0                     ; Null descriptor
    dw 0xFFFF,0,0x9A00,0x00CF      ; 32-bit code (0x08)
    dw 0xFFFF,0,0x9200,0x008F      ; Flat data    (0x10)
    dw 0xFFFF,0,0x9A00,0x00AF      ; 64-bit code (0x18)
ap_gdt_end:

ap_gdt_ptr:
    dw ap_gdt_end - ap_gdt - 1     ; GDT limit
    dd ap_gdt                       ; GDT base (linear address, works since trampoline is identity-mapped)

; Pad to known size (keep data area at 0x8FF0-0x8FFF accessible)
