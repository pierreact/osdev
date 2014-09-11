[BITS 16]
[SECTION .text]
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
asm16_IVT_setup:                                        ;
                                                        ;
                                                        ; The IVT by default is located from 0x0 (BASE) to 0x400.
	                                                    ; Each interrupt maps to a memory pointer on the IVT and is made of 4 bytes.
	                                                    ; 2 bytes for the segment, 2 bytes for the offset.
                                                        ;
mov ax, 0                                               ; Working on address 0x0000
mov es, ax                                              ;
mov bx, 0                                               ;
                                                        ;
cli                                                     ;
                                                        ;
.write:                                                 ;
    mov [es:bx], 	word asm16_isr_handler              ; Points all interrupts to the function _isr_handler.
    mov [es:bx +2], word 0                              ;
                                                        ;
    add bx, 4                                           ; 4 bytes per interrupt.
    cmp bx, 0x400 - 4                                   ; 256 x 4 = 0x400. Last vector for which an address 
    jb .write                                           ; going to asm16_isr_handler will be defined
                                                        ;
    mov [es:bx], 	word asm16_isr_intFFh               ; Points interrupt 0xFF to the function _isr_intFFh.
    mov [es:bx +2], word 0                              ;
                                                        ;
sti                                                     ;
ret                                                     ;
                                                        ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Only accepted ISR in 16 bits mode.
asm16_isr_intFFh:                                       ;
	mov si, msg_16bits_irq_called                       ;
	call asm16_display_writestring                      ;
                                                        ;
	iret                                                ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
asm16_isr_handler:                                      ;
                                                    	; INTERRUPT CATCHED
	mov ax, 0xB800                                      ;
	mov es, ax                                          ;
                                                        ;
	xor bx, bx                                          ;
	mov [es:bx],     byte 'I'                           ;
	mov [es:bx +2],  byte 'N'                           ;
	mov [es:bx +4],  byte 'T'                           ;
	mov [es:bx +6],  byte 'E'                           ;
	mov [es:bx +8],  byte 'R'                           ;
	mov [es:bx +10], byte 'R'                           ;
	mov [es:bx +12], byte 'U'                           ;
	mov [es:bx +14], byte 'P'                           ;
	mov [es:bx +16], byte 'T'                           ;
	mov [es:bx +18], byte ' '                           ;
 	mov [es:bx +20], byte 'C'                           ;
	mov [es:bx +22], byte 'A'                           ;
	mov [es:bx +24], byte 'T'                           ;
	mov [es:bx +26], byte 'C'                           ;
	mov [es:bx +28], byte 'H'                           ;
	mov [es:bx +30], byte 'E'                           ;
	mov [es:bx +32], byte 'D'                           ;
	mov [es:bx +34], byte ' '                           ;
	mov [es:bx +36], byte 'K'                           ;
	mov [es:bx +38], byte '1'                           ;
	mov [es:bx +40], byte '6'                           ;
	mov [es:bx +42], byte ' '                           ;
	mov [es:bx +44], byte 'P'                           ;
	mov [es:bx +46], byte 'A'                           ;
	mov [es:bx +48], byte 'N'                           ;
	mov [es:bx +50], byte 'I'                           ;
	mov [es:bx +52], byte 'C'                           ;
	mov [es:bx +54], byte '!'                           ;
                                                        ;
	                                                    ; Stops the O.S. with no come back.
	cli                                                 ; Disable interrupts
	hlt                                                 ; Halt.
                                                        ;
	iret                                                ; Will never be reached but just for completeness.
                                                        ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
[SECTION .data]                                         ;
msg_16bits_irq_called: db 'K16 - 16 bits interrupt called, IVT is initialized.', 0


