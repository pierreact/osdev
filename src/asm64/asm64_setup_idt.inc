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
                                                        ;
    mov rax, asm64_divide_zero_handler                  ; ISR Vector 0x00 Divide by zero
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_debug_handler                        ; ISR Vector 0x01 Debug
    call asm64_creategate_fault                         ; Create fault gate.
                                                        ;
    mov rax, asm64_nmi_handler                          ; ISR Vector 0x02 NMI
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x03
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x04
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x05
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x06
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x07
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x08
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x09
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x0a
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x0b
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x0c
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x0d
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x0e
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x0f
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x10
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x11
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x12
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x13
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x14
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x15
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x16
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x17
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x18
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x19
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x1a
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x1b
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x1c
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x1d
    call asm64_creategate_interrupt                     ; Create fault gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x1e
    call asm64_creategate_interrupt                     ; Create Interrupt gate.
                                                        ;
    mov rax, asm64_default_int_handler                  ; ISR Vector 0x1f
    call asm64_creategate_interrupt                     ; Create fault gate.
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
asm64_creategate_trap:                                  ;
                                                        ;
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
		;mov rax, asm64_default_int_handler             ; Address of the default int handler.
                                                        ;
		mov [rbx], word ax                              ; offset 0 to 15 of handler
                                                        ;
		mov [rbx +2], word cs                           ; Segment selector to reach the handler
                                                        ;
		mov [rbx +4], byte 00000000b                    ; Reserved, 3 least bits  are IST (disabled now)
		mov [rbx +5], byte 10001110b                    ; Attributes
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
    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
                                                        ;
	iretq                                               ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_divide_zero_handler:                              ;
	cli                                                 ;
                                                        ;
                                                        ;
    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
                                                        ;
	sti                                                 ;
	iretq                                               ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_debug_handler:                                    ;
	cli                                                 ;
                                                        ;
                                                        ;
    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
                                                        ;
	sti                                                 ;
	iretq                                               ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
asm64_nmi_handler:                                      ;
	cli                                                 ;
                                                        ;
                                                        ;
    call asm64_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
                                                        ;
	sti                                                 ;
	iretq                                               ;
                                                        ;
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

msg_idt64_set:          db 'K64 - IDT64 set.', 0
msg_default_64handler:  db 'K64 - Unknown Interrupt called!', 0
msg_numtimercalls:      db 'Number of calls to timer interrupt :', 0

kbstatus:	            db	0			                ; keyboard controller status
kbscancode:	            db	0			                ; scancode
                                                        ;
align 16                                                ;
idtptr64:                                               ;
	.idtptr64_limit: dw 0	                            ; limit
	.idtptr64_base:  dq 0	                            ; base



