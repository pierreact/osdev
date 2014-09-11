                                                        ;
[BITS 32]                                               ;
[SECTION .text]                                         ;
                                                        ;
%include "global_definitions.asm"                       ;
%include 'asm32/asm32_display.inc'                      ; Display functions
                                                        ;
                                                        ;
%define MMAP_BASE_ADDRESS 0x0                           ;
%define MMAP_LENGTH 0x8                                 ;
%define MMAP_TYPE 0x10                                  ;
                                                        ;
%define MMAP_TYPE_FREE 0x1                              ; Free
%define MMAP_TYPE_RESERVED 0x2                          ; Reserved, not available (e.g. system ROM, memory-mapped device)
%define MMAP_TYPE_ACPI_RECLAIM 0x3                      ; ACPI Reclaim Memory (usable by OS after reading ACPI tables)
%define MMAP_TYPE_ACPI_NVS 0x4                          ; ACPI NVS Memory (OS is required to save this memory between NVS
                                                        ;
%define MMAP_ENTRY_SIZE 24                              ; bytes
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
%define MSG_MMAP_BASE_OFFSET 40                         ; Points to the location of 'X':  K32 MMAP - Not free:  |0x000000000000000X - 0x0000000000000000|
%define MSG_MMAP_LENG_OFFSET 37                         ; Delta from Z to go to Y:        K32 MMAP - Not free:  |0xZ00000000000000X - 0x000000000000000Y|
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; 
asm32_display_memmap:                                   ; Display memory map the map is made of a number of 24 bytes entries
                                                        ;
mov esi, msg32_mmap_header                              ;
call asm32_display_writestring                          ; Print it.
                                                        ;
mov ecx, [esp+4]                                        ; [data_counter_mmap_entries]
mov ebx, [esp+8]                                        ; MEMMAP_START (0x8000)
                                                        ;
dec ecx                                                 ; We start from offset (0, 1, 2...)
mov edx, 0                                              ; Will count to edx
                                                        ;
.next:                                                  ;
                                                        ;
    mov eax, [ebx + MMAP_TYPE]                          ; What type is it?
    cmp eax, MMAP_TYPE_FREE                             ; Is it free memory?
    jne .not_free                                       ;
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
                                                        ;
    push msg32_mmap_free                                ; what buffer to write it to
    push ebx                                            ; Entry address
    call asm32_display_memmap_write_qword_from_mem      ;
                                                        ;
    mov esi, msg32_mmap_free                            ;
    call asm32_display_writestring                      ; Print it.
                                                        ;
    jmp .entry_end                                      ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
                                                        ;
.not_free:                                              ;
                                                        ;
    push msg32_mmap_notfree                             ; what buffer to write it to
    push ebx                                            ; Entry address
    call asm32_display_memmap_write_qword_from_mem      ;
                                                        ;
    mov esi, msg32_mmap_notfree                         ;
    call asm32_display_writestring                      ; Print it.
                                                        ;
.entry_end:                                             ;
                                                        ;
    inc edx                                             ; Next entry counter
                                                        ;
    add ebx, MMAP_ENTRY_SIZE                            ;
    cmp edx, ecx                                        ; Did we reach the number of entries?
    jbe .next                                           ; Didn't reach the number of entries? next!
                                                        ;
    ret 8                                               ; Back home
                                                        ;
                                                        ;
                                                        ;
                                                        ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
                                                        ;
                                                        ;
asm32_display_memmap_write_qword_from_mem:              ;
                                                        ;
                                                        ;
    pushad                                              ; = push eax, ecx, edx, ebx, esp, ebp, esi, edi (8 x 4 = 0x20)
    pushfd                                              ; = push EFLAGS ; 0x4 bytes (EFLAGS is 32 bits)
                                                        ;
    mov ebx, [esp+0x4+0x4+0x20]                         ; call Return address + EFLAGS + pushad... In other words, last pushed argument... Address of the entry
    mov edi, [esp+0x4+0x4+0x24]                         ; Destination buffer?
                                                        ;
    add edi, MSG_MMAP_BASE_OFFSET                       ; We start at the end of the base address representation in the buffer.
    call read_64bit_value_and_write_hex_to_buffer
                                                        ;
    add edi, MSG_MMAP_LENG_OFFSET                       ; + 37 in buffer to write at the length chars, end of the length in the buffer
    call read_64bit_value_and_write_hex_to_buffer

    popfd
    popad

    ret 8


;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------

read_64bit_value_and_write_hex_to_buffer:

mov ecx, 0

.loop:                                              ; 
                                                    ;
    ;-------------------------                      ; Step 1, lower half of the byte
    mov al, [ebx]                                   ; Address of the byte to show up.
    call translate_al_to_hex                        ; Translate it to hex
    mov [edi], al                                   ; Write it to the buffer
    dec edi                                         ; Get to the previous byte of the buffer
                                                    ;
    ;-------------------------                      ;
                                                    ; Step 2 higher half of the byte
    mov al, [ebx]                                   ; Load the byte in al again
    shr al, 4                                       ; set the higher half to become the lower half, ready to translate
    call translate_al_to_hex                        ; Translate it to hex
    mov [edi], al                                   ; Write it to the buffer
    dec edi                                         ; Get to the previous byte of the buffer
                                                    ;
    ;-------------------------                      ;
                                                    ;
    inc ebx                                         ; Next byte
    inc ecx                                         ; Increment couter
                                                    ;
    cmp ecx, 8                                      ; Is counter 8 (64 bits value is 8 bytes)?
    jb .loop                                        ; If not, next byte

    ret

;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------

translate_al_to_hex:

    and al, 0x0F                                    ; Zero the higher half
	cmp al, 9                                       ; Should it be numerical or alpha?
	ja .char

	add al, 48			                            ; ASCII 0 character
	jmp .done_hex

.char:
	add al, 55                                      ; 'A' - 10
.done_hex:
    ret



;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------


asm32_memmap_find_highest_entry:

    pushad                                              ; = push eax, ecx, edx, ebx, esp, ebp, esi, edi (8 x 4 = 0x20)
    pushfd                                              ; = push EFLAGS ; 0x4 bytes (EFLAGS is 32 bits)

    mov edx, 0                                          ; EDX is going to be used to keep the highest entry value
                                                        ;
    mov ebx, [esp+0x4+0x4+0x20]                         ; How many entries the memmap has?
    mov esi, [esp+0x4+0x4+0x24]                         ; Where is the memory map? (0x8000 normally)
    
    xor ecx, ecx

;    dec ebx                                             ; We start from 0


    .loop_highest:
        mov eax, [esi]                                      ; Load the higher half of the base address

        cmp eax, edx                                        ; Higher half of the base address compared to the highest higher half found.
        jb .next_highest                                            ; If below, next entry.

        mov [highest_mmap_entry], ecx 
        mov edx, eax

    .next_highest:
        
        add esi, MMAP_ENTRY_SIZE
        inc ecx
        cmp ebx, ecx ; Compare the max number of entries (ebx) to the currentely analyzed entry (ecx), rae we done?
        jne .loop_highest    ; Not? next entry

    popfd
    popad

    ret





;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------

; Depends on asm32_memmap_find_highest_entry to have been called already
asm32_get_last_mem_address:
    pushad
    pushfd

;-------------
    mov eax, [highest_mmap_entry]

    mov edx, MMAP_ENTRY_SIZE
    mul edx                            ; Result of the multiplication will be in EDX:EAX but this operation will stay in the 32 bits limits, so, eax
    add eax, MEMMAP_START

    mov esi, eax    ; Address of the highest memmap entry


;-------------

    clc
    mov eax, [esi]
    mov ebx, [esi+8]

    add eax, ebx

    mov [mem_amount+4], eax


;-------------
    

    mov ecx, [esi+4]
    mov edx, [esi+12]

    adc ecx, edx

    mov [mem_amount], ecx

    popfd
    popad

    ret





[SECTION .data]


msg32_mmap_header:  db "K32 - MMAP Type       | Base Address      | Length            |", 0
msg32_mmap_free:    db "K32 - MMAP Free entry:|0x0000000000000000 - 0x0000000000000000|", 0
msg32_mmap_notfree: db "K32 - MMAP Not free:  |0x0000000000000000 - 0x0000000000000000|", 0
msg32_separator:    db "-------------", 0

