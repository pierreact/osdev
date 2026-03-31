; SYSCALL/SYSRET infrastructure for x86-64
; Included from kmain.s in the 64-bit code section.

EXTERN syscall_dispatch

;----------------------------------------------------------------------------------------------------------------------------------------
; syscall_init - Configure SYSCALL/SYSRET MSRs
;
; Sets up IA32_STAR, IA32_LSTAR, IA32_SFMASK and enables EFER.SCE.
; Must be called after GDT64 is loaded and tss_init is done.
; SWAPGS kernel GS base is set to &percpu[0] (BSP).
;----------------------------------------------------------------------------------------------------------------------------------------
syscall_init:
    push rbx

    ; Enable SCE (System Call Extensions) in EFER
    mov ecx, 0xC0000080                                 ; IA32_EFER
    rdmsr
    bts eax, 0                                          ; SCE = bit 0
    wrmsr

    ; IA32_STAR: SYSCALL CS = 0x08, SYSRET CS base = 0x18
    ; SYSRET 64-bit: CS = 0x18+16 = 0x28, SS = 0x18+8 = 0x20
    mov ecx, 0xC0000081                                 ; IA32_STAR
    xor edx, edx
    mov edx, 0x00180008                                 ; [63:48]=0x0018, [47:32]=0x0008
    xor eax, eax                                        ; [31:0] = 0 (unused in 64-bit)
    wrmsr

    ; IA32_LSTAR: SYSCALL entry point address
    mov ecx, 0xC0000082                                 ; IA32_LSTAR
    lea rax, [rel syscall_entry]
    mov rdx, rax
    shr rdx, 32                                         ; EDX = high 32 bits
    wrmsr

    ; IA32_SFMASK: clear IF (bit 9) on SYSCALL entry
    mov ecx, 0xC0000084                                 ; IA32_SFMASK
    mov eax, 0x200                                      ; Mask IF
    xor edx, edx
    wrmsr

    ; Set IA32_KERNEL_GS_BASE to &percpu[0] for SWAPGS
    mov ecx, 0xC0000102                                 ; IA32_KERNEL_GS_BASE
    lea rax, [rel percpu]
    mov rdx, rax
    shr rdx, 32
    wrmsr

    pop rbx
    ret

;----------------------------------------------------------------------------------------------------------------------------------------
; syscall_entry - SYSCALL lands here from ring 3
;
; On entry (set by CPU):
;   RCX = user RIP
;   R11 = user RFLAGS
;   RAX = syscall number
;   RDI, RSI, RDX, R10, R8, R9 = arguments (Linux convention)
;   RSP = still user stack (SYSCALL does NOT switch stacks)
;   IF = 0 (cleared by SFMASK)
;----------------------------------------------------------------------------------------------------------------------------------------
syscall_entry:
    swapgs                                              ; GS base = &percpu[cpu_idx]

    ; Save user RSP, load kernel RSP
    mov [gs:24], rsp                                    ; percpu.user_rsp (offset 24)
    mov rsp, [gs:8]                                     ; percpu.stack_top (offset 8)

    ; Save user context on kernel stack
    push rcx                                            ; User RIP
    push r11                                            ; User RFLAGS

    ; Save callee-saved registers (C ABI)
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; Re-enable interrupts in kernel mode
    sti

    ; Shift arguments for C calling convention:
    ; syscall_dispatch(nr, a1, a2, a3, a4, a5)
    ; Syscall:  RAX=nr, RDI=a1, RSI=a2, RDX=a3, R10=a4, R8=a5
    ; C ABI:    RDI=nr, RSI=a1, RDX=a2, RCX=a3, R8=a4,  R9=a5
    mov r9, r8                                          ; a5
    mov r8, r10                                         ; a4
    mov rcx, rdx                                        ; a3
    mov rdx, rsi                                        ; a2
    mov rsi, rdi                                        ; a1
    mov rdi, rax                                        ; nr
    call syscall_dispatch                               ; Returns result in RAX

    ; Disable interrupts before returning to user mode
    cli

    ; Restore callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    pop r11                                             ; User RFLAGS
    pop rcx                                             ; User RIP

    ; Restore user RSP
    mov rsp, [gs:24]                                    ; percpu.user_rsp

    swapgs                                              ; Restore user GS base
    o64 sysret                                          ; Return to ring 3
