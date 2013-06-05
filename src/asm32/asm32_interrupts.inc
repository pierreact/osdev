[BITS 32]
[SECTION .text]

;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Definitions.
                                                        ; 
%define PIC1 0x20                                       ; Port of the first Programmable Interrupt Controller
%define PIC2 0x0A                                       ; Port of the second Programmable Interrupt Controller
                                                        ; 
%define PIC1_COMMAND PIC1                               ; Command port of the first PIC
%define PIC1_DATA (PIC1 + 1)                            ; Data/IMR (Interrupt Mask Register) port of the first PIC
                                                        ;
%define PIC2_COMMAND PIC2                               ; Command port of the second PIC
%define PIC2_DATA (PIC2 + 1)                            ; Data/IMR (Interrupt Mask Register) port of the second PIC
                                                        ;
%define PIC_EOI 0x20                                    ; End-of-interrupt command code
                                                        ;
                                                        ;
%define ICW1 00010001b                                  ; Enables initialization mode and says we'll set ICW4
%define IRQ0 0x20                                       ; IRQ0 to be mapped to interrupt vector 0x20
%define IRQ8 0x28                                       ; IRQ8 to be mapped to interrupt vector 0x28

              
%define IDT32_BASE      0x7000                          ;

                                          ;
;----------------------------------------------------------------------------------------------------------------------------------------



                                                        ; Remaping the Programmable Interrupt Controller
                                                        ; so it doesn't overlap on the IDT
asm32_setup_pic:                                        ;
; ICW1: Begin initialization                        ;
                                                        ;
    mov al, ICW1                                        ;
    out PIC1_COMMAND, al                                ; Sets the primary PIC in initialiation mode. It is now waiting for our commands.
    out PIC2_COMMAND, al                                ; Sets the secondary PIC in initialiation mode. It is now waiting for our commands.

; ICW2: Map IRQ base address interrupts
    ; Primary PIC:
    mov al, IRQ0                                        ;
    out PIC1_DATA, al                                   ; Sets the primary PIC controller base vector for IRQ0

    ; Secondary PIC:
    mov al, IRQ8
    out PIC2_DATA, al                                   ; Sets the secondary PIC controller base vector for IRQ8

; ICW3: Sets the IR line to connect both PICs (They use cascading)
    ; Primary PIC:
    mov al, 0100b                                       ; second bit, (IR line 2)
    out PIC1_DATA, al

    ; Secondary PIC:
    mov al, 0010b                                       ; second bit, (IR line 2)
    out PIC2_DATA, al

; ICW4: Sets x86 mode.
    mov al, 1

    out PIC1_DATA, al
    out PIC2_DATA, al
    
; PIC initialization done, sets data registers to 0
    mov al, 0

    out PIC1_DATA, al
    out PIC2_DATA, al

    ret

;----------------------------------------------------------------------------------------------------------------------------------------


asm32_pic_send_eoi:                                      ; Send End of interrupt to the PIC, THIS HAS TO BE RUN AT THE END OF EACH ISR.
    push eax
    mov al,0x20
    out PIC2_COMMAND, al
    out PIC1_COMMAND, al 
    pop eax
    ret


;----------------------------------------------------------------------------------------------------------------------------------------


                                                        ; 00000000  00 00 8e 00 08 00 9c 12  00 00 8e 00 08 00 9c 12  |................|

asm32_setup_idt:
                                                        ;
	mov ebx, IDT32_BASE                                     ; from 0x7000 to 0x7800 in memory,
    mov [idtptr32_base], dword ebx                      ; Base of IDT is.... 0!
	cli                                                 ; Disable interrupts
                                                        ;
	.write:                                             ; Loop it up!
                                                        ;
		                                                ; Byte 0 and 1 offset 16 to 31 of ISR
		                                                ; Byte 2       Type and attributes
		                                                ; Byte 3       Reserved, at 0
		                                                ; Byte 4 and 5 Code selector
		                                                ; Byte 6 and 7 offset 1 to 15 of ISR
                                                        ;
		mov eax, asm32_default_int_handler              ; Address of the default int handler.
                                                        ;
		mov [ebx], word ax                           ; offset 0 to 15 of handler
                                                       ;
		mov [ebx +2], word 0x0008                       ; Segment selector to reach the handler
                                                        ;
		mov [ebx +4], byte 0                            ; Reserved
		mov [ebx +5], byte 10001110b                    ; Attributes
                                                        ;
                                                        ;
		shr eax, 16                                     ; Shifts the high part of the address to ax.
		mov [ebx +6], word ax                              ; offset 16..31 of handler

		add ebx, 8                                      ; 8 bytes per interrupt.
		cmp ebx, 0x800 + IDT32_BASE                                  ; 256 x 8 = 0x800. 
                                                        ; Last vector for which an address going to handler will be defined
		jb .write                                       ; Did we reach the last gate? If not, write the next one
                                                        ;
		mov [idtptr32_limit], word bx                   ; Writes the size to the limit of idtptr

        lidt [idtptr32]

	sti

    push esi	

    mov esi, msg_idt32_set                              ; Load message informing IDT is set
    call asm32_display_writestring 

    pop esi

	ret


asm32_default_int_handler:

    	cli
	push ebx
    push esi

;	mov ebx, 0xB8160
;	mov [ebx], byte 'W'

;    mov esi, msg_default_handler                                    ; Load message informing an interrupt was called.
;    call asm32_display_writestring 

    pop esi
	pop ebx

    call asm32_pic_send_eoi                             ; Tells the PIC we're done servicing the interrupt.
	sti
	iret



[SECTION .data]

msg_idt32_set       db 'K32 - IDT32 set.', 0
msg_default_handler db 'K32 - Default Interrupt handler and IDT32 set (Interrupt called.).', 0


idtptr32:
	idtptr32_limit: dw 0	; limit
	idtptr32_base:  dd 0	; base

;	idtptr32_limit: dw 0	; limit

;idtentry:
;	offset_1:		dw 0 ; offset bits 0..15
;	seg_selector:	db 0 ; Selector for a segment in the GDT.
;	zero:			db 0 ;unused, set to 0
;	type_attribute:	db 0 ; Types and attibutes
;	offset_2:		dw 0 ; offset bits 16..31





                                                        ; Remaping the Programmable Interrupt Controller
                                                        ; so it doesn't overlap on the IDT
;asm32_setup_pic:                                         ;
;    
;    remap_pic:
;       cli
;        mov  al,0x11                                      ; put both 8259s in init mode
;        out  0x20,al
;        out  0xA0,al
;
;        mov  al,0x20                                      ; remap pic irq0-irq7 -> int 0x20-27
;        out  0x21,al
;        mov  al,0x28
;
;        out  0xA1,al                                      ; remap pic irq8-irq15 -> int 0x28-30
;        mov  al,4                                         ; icw3 pic1(master)
;        out  0x21,al                                      ; bit 2=1: irq2 is the slave
;        mov  al,2                                         ; icw3 pic2
;        out  0xA1,al                                      ; bit 1=1: slave id is 2
;        mov  al,1                                         ; icw4 to both controllers
;        out  0x21,al                                      ; bit 0=1: 8086 mode
;        out  0xA1,al 
;    mask_irqs:    
;        cli
;        mov  al,255                                       ; mask all irqs
;        out  0xa1,al
;        out  0x21,al 
;        ret                                                 ; Function ends.
;
;    unmask_irqs:
;        mov  al,0                                         ; unmask irq's 
;        out  0xA1,al
;        out  0x21,al
;        sti 
;    ret                                                 ; Function ends.

