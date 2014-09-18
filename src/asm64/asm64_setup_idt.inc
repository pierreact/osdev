[BITS 64]
[SECTION .text]

;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
asm64_pic_send_eoi:                                     ; Send End of interrupt to the PIC, THIS HAS TO BE RUN AT THE END OF EACH ISR.
    push rax                                            ;
    mov al,0x20                                         ;
    out PIC2_COMMAND, al                                ;
    out PIC1_COMMAND, al                                ;
    pop rax                                             ;
    ret                                                 ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_setup_idt:                                        ;
                                                        ;
	mov rbx, IDT64_BASE                                 ; from 0x7000 to 0x8000 in memory,
    mov [idtptr64.idtptr64_base], qword rbx             ; 
	cli                                                 ; Disable interrupts
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
                                                        ; Exceptions
                                                        ; http://wiki.osdev.org/Exceptions
                                                        ;
    mov rax, asm64_divide_zero_handler                  ; ISR Vector 0x00 Divide by zero
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_debug_handler                        ; ISR Vector 0x01 Debug
    call asm64_creategate_trap                          ; Create trap gate.
                                                        ;
    mov rax, asm64_nmi_handler                          ; ISR Vector 0x02 NMI
    call asm64_creategate_interrupt                     ; Create interrupt gate.
                                                        ;
    mov rax, asm64_breakpoint_int_handler               ; ISR Vector 0x03 Breakpoint
    call asm64_creategate_trap                          ; Create trap gate.
                                                        ;
    mov rax, asm64_overflow_int_handler                 ; ISR Vector 0x04 Overflow
    call asm64_creategate_trap                          ; Create trap gate.
                                                        ;
    mov rax, asm64_brange_int_handler                   ; ISR Vector 0x05 Bound range exceeded
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_invopcd_int_handler                  ; ISR Vector 0x06 Invalid Opcode
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_devna_int_handler                    ; ISR Vector 0x07 Device not available
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_doublef_int_handler                  ; ISR Vector 0x08 Double fault
    call asm64_creategate_abort                         ; Create abort gate.
                                                        ;
    mov rax, asm64_cso_int_handler                      ; ISR Vector 0x09 Coprocessor Segment Overrun
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_invtss_int_handler                   ; ISR Vector 0x0a Invalid TSS
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_snp_int_handler                      ; ISR Vector 0x0b Segment not present
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_ssf_int_handler                      ; ISR Vector 0x0c Stack segment fault.
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_gpf_int_handler                      ; ISR Vector 0x0d General protection fault
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_pf_int_handler                       ; ISR Vector 0x0e Page fault
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_rsv_int_handler                      ; ISR Vector 0x0f Reserved
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_fpu_int_handler                      ; ISR Vector 0x10 x87 Floating-Point Exception
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_achk_int_handler                     ; ISR Vector 0x11 Alignement check
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_mce_int_handler                      ; ISR Vector 0x12 Machine Check
    call asm64_creategate_abort                         ; Create abort gate.
                                                        ;
    mov rax, asm64_simd_int_handler                     ; ISR Vector 0x13 SIMD Floating-Point Exception
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_virt_int_handler                     ; ISR Vector 0x14 Virtualization Exception
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_rsv_int_handler                      ; ISR Vector 0x15 Reserved
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_rsv_int_handler                      ; ISR Vector 0x16 Reserved
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_rsv_int_handler                      ; ISR Vector 0x17 Reserved
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_rsv_int_handler                      ; ISR Vector 0x18 Reserved
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_rsv_int_handler                      ; ISR Vector 0x19 Reserved
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_rsv_int_handler                      ; ISR Vector 0x1a Reserved
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_rsv_int_handler                      ; ISR Vector 0x1b Reserved
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_rsv_int_handler                      ; ISR Vector 0x1c Reserved
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_rsv_int_handler                      ; ISR Vector 0x1d Reserved
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_sec_int_handler                      ; ISR Vector 0x1e Security Exception
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_rsv_int_handler                      ; ISR Vector 0x1f Reserved
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
                                                        ; IRQs
                                                        ;
    mov rax, asm64_isr_timer_handler                    ; ISR Vector 0x20 (IRQ0) PIT
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_isr_keyboard_handler                 ; ISR Vector 0x21 (IRQ1) Keyboard
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x23 (IRQ3) COM2
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x23 (IRQ3) COM2
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x24 (IRQ4) COM1
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x25 (IRQ5) LPT2
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x26 (IRQ6) Floppy
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x27 (IRQ7) LPT1 / Unreliable "spurious" interrupt (usually)
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x28 (IRQ8) Clock
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x29 (IRQ9) Free for peripherals / legacy SCSI / NIC
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x2A (IRQ10) Free for peripherals / SCSI / NIC
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x2B (IRQ11) Free for peripherals / SCSI / NIC
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x2C (IRQ12) PS2 Mouse
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x2D (IRQ13) FPU
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x2E (IRQ14) Primary ATA disk
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x2F (IRQ15) Secondary ATA disk
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
                                                        ; Software Interupts
                                                        ;
    mov rax, asm64_softwareint_handler                  ; ISR Vector 0x30 Software interrupt
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x31 Software interrupt
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x32 Software interrupt
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
                                                        ;
    sub rbx, IDT64_BASE                                 ;
	mov [idtptr64.idtptr64_limit], word bx              ; Writes the size to the limit of idtptr
                                                        ;
    lidt [idtptr64]                                     ; Finally, load IDT64
                                                        ;
	sti                                                 ; Enable interrupts
                                                        ; Pray it doesn't crash... nah.
                                                        ;
    push rsi                                            ;
    mov rsi, msg_idt64_set                              ;
    call asm64_display_writestring                      ;
    pop rsi                                             ;
                                                        ;
	ret                                                 ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
asm64_creategate_fault:                                 ;
asm64_creategate_interrupt:                             ;
asm64_creategate_abort:                                 ;
    mov dl, 10001110b                                   ; Attributes
    jmp asm64_idt_entry

asm64_creategate_trap:                                  ;
    mov dl, 10001111b                                   ; Attributes
                                                        ;
asm64_idt_entry:
                                                        ; In 64 bits, the structure of an IDT descriptor, 
                                                        ; pointing to the ISR is a bit different.
                                                        ;
		                                                ; Byte 0 and 1  Offset low bits (0..15) of ISR
		                                                ; Byte 2 and 3  Selector
		                                                ; Byte 4        Reserved, at 0
		                                                ; Byte 5        Type and attributes (Identical to 32 bits)
		                                                ; Byte 6 and 7  Offset middle bits (16..31) of ISR
		                                                ; Byte 8 to 11  Offset high bits (32..63) of ISR
                                                        ; Byte 12 to 15 Reserved, at 0
                                                        ;
                                                        ; rax has the address of the default int handler.
                                                        ;
		mov [rbx], word ax                              ; offset 0 to 15 of handler
                                                        ;
		mov [rbx +2], word cs                           ; Segment selector to reach the handler
                                                        ;
		mov [rbx +4], byte 00000000b                    ; Reserved, 3 least bits  are IST (disabled now)
		mov [rbx +5], dl                                ; Attributes
                                                        ; This is a 64 bits interrupt gate as per intel manual 3a part 1
                                                        ; section 3.5 SYSTEM DESCRIPTOR TYPES
                                                        ;
		shr rax, 16                                     ; Shifts the high part of the address to ax.
		mov [rbx +6], word ax                           ; offset 16..31 of handler
                                                        ;
		shr rax, 16                                     ; Shifts the high part of the address to eax.
		mov [rbx +8], dword eax                         ; offset 32..63 of handler
                                                        ;
        xor eax, eax                                    ;
        mov [rbx +12], dword eax                        ; Reserved, at 0
                                                        ;
		add rbx, 16                                     ; 16 bytes per interrupt.
                                                        ;
        ret                                             ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
asm64_default_int_handler:                              ;
                                                        ;
    mov rsi, msg_default_64handler                      ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
cli
hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
                                                        ;
	iretq                                               ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_divide_zero_handler:                              ; 0x0
	cli                                                 ;
    mov rsi, msg_divide_zero_64handler                   ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
cli
hlt
                                                        ;
    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
                                                        ;
	sti                                                 ;
	iretq                                               ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_debug_handler:                                    ; 0x1
	cli                                                 ;
                                                        ;
    mov rsi, msg_debug_64handler                   ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
cli
hlt
                                                        ;
    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
                                                        ;
	sti                                                 ;
	iretq                                               ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_nmi_handler:                                      ; 0x2
	cli                                                 ;
                                                        ;
    mov rsi, msg_nmi_64handler                   ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
cli
hlt
                                                        ;
    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
                                                        ;
	sti                                                 ;
	iretq                                               ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_breakpoint_int_handler:                           ; 0x3
    mov rsi, msg_breakpoint_64handler                   ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq

;----------------------------------------------------------------------------------------------------------------------------------------
asm64_overflow_int_handler:                                  ; 0x4
    mov rsi, msg_overflow_64handler                          ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_brange_int_handler:                           ; 0x5
    mov rsi, msg_brange_64handler                   ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq

;----------------------------------------------------------------------------------------------------------------------------------------
asm64_invopcd_int_handler:                           ; 0x6
    mov rsi, msg_invopcd_64handler                   ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq

;----------------------------------------------------------------------------------------------------------------------------------------
asm64_devna_int_handler:                           ; 0x7
    mov rsi, msg_devna_64handler                   ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq

;----------------------------------------------------------------------------------------------------------------------------------------
asm64_doublef_int_handler:                              ; 0x8
    mov rsi, msg_doublef_64handler                      ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq


;----------------------------------------------------------------------------------------------------------------------------------------
asm64_cso_int_handler:                              ; 0x9
    mov rsi, msg_cso_64handler                      ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_invtss_int_handler:                              ; 0xA
    mov rsi, msg_invtss_64handler                      ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq

;----------------------------------------------------------------------------------------------------------------------------------------
asm64_snp_int_handler:                              ; 0xb
    mov rsi, msg_cnp_64handler                      ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq

;----------------------------------------------------------------------------------------------------------------------------------------
asm64_ssf_int_handler:                              ; 0xc
    mov rsi, msg_ssf_64handler                      ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq

;----------------------------------------------------------------------------------------------------------------------------------------
asm64_gpf_int_handler:                                  ; 0xD
    mov rsi, msg_gpf_64handler                          ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq


;----------------------------------------------------------------------------------------------------------------------------------------
asm64_pf_int_handler:                                  ; 0xE
    mov rsi, msg_pf_64handler                          ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq

;----------------------------------------------------------------------------------------------------------------------------------------
asm64_fpu_int_handler:                                  ; 0x10
    mov rsi, msg_fpu_64handler                          ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq

;----------------------------------------------------------------------------------------------------------------------------------------
asm64_achk_int_handler:                                  ; 0x11
    mov rsi, msg_achk_64handler                          ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_mce_int_handler:                                  ; 0x12
    mov rsi, msg_mce_64handler                          ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_simd_int_handler:                                  ; 0x13
    mov rsi, msg_simd_64handler                          ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_virt_int_handler:                                  ; 0x14
    mov rsi, msg_virt_64handler                          ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_sec_int_handler:                                  ; 0x1E
    mov rsi, msg_sec_64handler                          ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_rsv_int_handler:                                  ; All reserved exceptions
    mov rsi, msg_rsv_64handler                          ; Load message informing an interrupt was called.
    call asm64_display_writestring                      ;
                                                        ;
    cli
    hlt

    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
    iretq
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_isr_timer_handler:                                ;
                                                        ;
    mov rax, [GLOBAL_OS_COUNTER]                        ;
    inc rax                                             ;
    mov [GLOBAL_OS_COUNTER], rax                        ;
                                                        ;
    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
                                                        ;
	iretq                                               ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_isr_keyboard_handler:                             ;
                                                        ;
	in	al, 0x64		                                ; read controller status
	mov	[kbstatus], al		                            ; save controller status
                                                        ;
	test al, 0x01		                                ; output-buffer empty?
	jz	.ignore			                                ; yes, nothing to read
                                                        ;
	in	al, 0x60		                                ; read the new scancode
	mov	[kbscancode], al		                        ; save the new scancode
                                                        ;
	test byte [kbstatus], 0xC0		                    ; parity/timeout errors?
	jnz	.resend			                                ; yes, a retry is needed
	jmp	.end			                                ; else ready to process
                                                        ;
.resend:	                                            ; TODO: our error-recovery routine will go here
                                                        ;
.ignore:                                                ;
    mov byte [kbscancode], 0x00		                    ; substitute a null byte
    .end:					                            ; return to do handling
                                                        ;
                                                        ;
    int 0x30                                            ;
                                                        ;
    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
                                                        ;
	iretq                                               ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_softwareint_handler:                              ;
                                                        ;
    xor rbx, rbx                                        ;
    mov bl, [kbscancode]                                ; Load kernel end address.
                                                        ;
;    mov rbx, [GLOBAL_OS_COUNTER]                        ; Load kernel end address.
    call asm64_display_make_string_from_hex             ; Make is hex string
    mov rsi, rbx                                        ;
    call asm64_display_writestring                      ;
                                                        ;
    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
                                                        ;
	iretq                                               ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
[SECTION .data]

msg_idt64_set:              db 'K64 - IDT64 set.', 0
msg_default_64handler:      db 'K64 - Unknown Interrupt called!      PANIC [Call Mickey Mouse!] Stopping the OS.', 0
msg_divide_zero_64handler:  db 'K64 - Divide by Zero!                    PANIC Exception (0x00) Stopping the OS.', 0
msg_debug_64handler:        db 'K64 - Debug!                             PANIC Exception (0x01) Stopping the OS.', 0
msg_nmi_64handler:          db 'K64 - NMI!                               PANIC Exception (0x02) Stopping the OS.', 0
msg_breakpoint_64handler:   db 'K64 - Breakpoint!                        PANIC Exception (0x03) Stopping the OS.', 0
msg_overflow_64handler:     db 'K64 - Overflow!                          PANIC Exception (0x04) Stopping the OS.', 0
msg_brange_64handler:       db 'K64 - Bound Range Exceeded!              PANIC Exception (0x05) Stopping the OS.', 0
msg_invopcd_64handler:      db 'K64 - Invalid Opcode!                    PANIC Exception (0x06) Stopping the OS.', 0
msg_devna_64handler:        db 'K64 - Device not available!              PANIC Exception (0x07) Stopping the OS.', 0
msg_doublef_64handler:      db 'K64 - Double fault!                      PANIC Exception (0x08) Stopping the OS.', 0
msg_cso_64handler:          db 'K64 - Coproc Segment Overrun!            PANIC Exception (0x09) Stopping the OS.', 0
msg_invtss_64handler:       db 'K64 - Invalid TSS!                       PANIC Exception (0x0A) Stopping the OS.', 0
msg_cnp_64handler:          db 'K64 - Segment not present!               PANIC Exception (0x0B) Stopping the OS.', 0
msg_ssf_64handler:          db 'K64 - Stack Segment Fault!               PANIC Exception (0x0C) Stopping the OS.', 0
msg_gpf_64handler:          db 'K64 - General protection fault!          PANIC Exception (0x0D) Stopping the OS.', 0
msg_pf_64handler:           db 'K64 - Page Fault!                        PANIC Exception (0x0E) Stopping the OS.', 0
msg_fpu_64handler:          db 'K64 - Floating Point Exception!          PANIC Exception (0x10) Stopping the OS.', 0
msg_achk_64handler:         db 'K64 - Alignement Check Exception!        PANIC Exception (0x11) Stopping the OS.', 0
msg_mce_64handler:          db 'K64 - Machine Check Exception!           PANIC Exception (0x12) Stopping the OS.', 0
msg_simd_64handler:         db 'K64 - SIMD Floating-Point Exception!     PANIC Exception (0x13) Stopping the OS.', 0
msg_virt_64handler:         db 'K64 - Virtualization Exception!          PANIC Exception (0x14) Stopping the OS.', 0
msg_sec_64handler:          db 'K64 - Machine Check Exception!           PANIC Exception (0x1E) Stopping the OS.', 0
msg_rsv_64handler:          db 'K64 - RESERVED EXEPTION!                 PANIC Exception (0x??) Stopping the OS.', 0
msg_numtimercalls:          db 'Number of calls to timer interrupt :', 0

kbstatus:	            db	0			                ; keyboard controller status
kbscancode:	            db	0			                ; scancode
                                                        ;
align 16                                                ;
idtptr64:                                               ;
	.idtptr64_limit: dw 0	                            ; limit
	.idtptr64_base:  dq 0	                            ; base



