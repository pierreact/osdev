
%include 'asm32/asm32_setup_64bits_paging_definitions.inc'

[BITS 32]
[SECTION .text]
%include 'asm32/asm32_display.inc'                      ; Screen functions.

setup_64_bits_paging_structures_pml4:
    push dword [PML4T_LOCATION]
    call create_empty_table
    ret


create_empty_table:
    pushad                                              ; = push eax, ecx, edx, ebx, esp, ebp, esi, edi (8 x 4 = 0x20)
    pushfd                                              ; = push EFLAGS ; 0x4 bytes (EFLAGS is 32 bits)

    mov edi, [esp+0x4+0x4+0x20] ; [PML4T_LOCATION]                              ; Clear the whole PML4 table


;    call paging_clean_entry



    xor eax, eax
    mov ecx, 1024                                       ; PML4T is made of 512 (0x1FF) PML4E of 64 bits at max.
                                                        ; for a total of 4KB, 0x1000 bytes 
    rep stosd                                           ; Write the empty PML4T


    mov eax, [PAGING_LOCATION_END]
    add eax, 4096
    mov [PAGING_LOCATION_END], eax

    popfd
    popad

    ret 4



;paging_clean_entry:
;    pushad                                              ; = push eax, ecx, edx, ebx, esp, ebp, esi, edi (8 x 4 = 0x20)
;    pushfd                                              ; = push EFLAGS ; 0x4 bytes (EFLAGS is 32 bits)
;
;    mov ebx, paging_entry
;    mov eax, 0
;
;    mov [ebx], eax
;    mov [ebx+4], eax
;
;    popfd
;    popad
;
;    ret

display_current_page_base_addr:
    push ebx
    mov ebx, [CURRENTLY_PAGED_ADDRESS]
    call asm32_display_make_string_from_hex
    mov esi, ebx
    call asm32_display_writestring
    pop ebx
    ret


display_reached_paging_types:
    push ebx
    mov ebx, [REACHED_PHY_MEM_ADDR_PAGINGTYPES]
    call asm32_display_make_string_from_hex
    mov esi, ebx
    call asm32_display_writestring
    pop ebx
    ret

did_i_reach_last_pml4e:
    clc
    push eax                                                 ; Clear Carry
    mov eax, [REACHED_PHY_MEM_ADDR_PAGINGTYPES]         ; Did we reach the last PML4E?
    and eax, REACHED_LAST_PML4E                         ; Last PML4E reached?
    cmp eax, REACHED_LAST_PML4E                         ;
    jne .pml4e_not_set
    stc                                                 ; Reached last PDPE
    jmp .end

.pml4e_not_set:
    clc
.end:
    pop eax
    ret


reached_last_pml4e:
                                                        ;--------------------------------------------------------------------------------
                                                        ; Indicate we're at the last PML4E
    push eax                                            ; Save eax
    mov eax, [REACHED_PHY_MEM_ADDR_PAGINGTYPES]         ; Load our bitmap of reached addresses
    or eax, REACHED_LAST_PML4E                          ; Check global definitions.asm,  (Bit 4: Last PML4E)
    mov [REACHED_PHY_MEM_ADDR_PAGINGTYPES], eax         ; Write down the result
    pop eax                                             ; Restore eax
    ret



;----------------------------------------------------------------------------------------------------------------------------------------



did_i_reach_last_pdpe:
    push eax                                                 ; Clear Carry
    mov eax, [REACHED_PHY_MEM_ADDR_PAGINGTYPES]         ; Did we reach the last PDPE?
    and eax, REACHED_LAST_PDPE                         ; Last PDPE reached?
    cmp eax, REACHED_LAST_PDPE                         ;
    jne .pdpe_not_set
    stc                                                 ; Reached last PDPE
    jmp .end

.pdpe_not_set:
    clc

.end:



    pop eax
    ret


reached_last_pdpe:
        mov eax, [REACHED_PHY_MEM_ADDR_PAGINGTYPES]     ; Write down the result
        or eax, REACHED_LAST_PDPE                       ; Check global definitions.asm,  (Bit 3: Last PDPE)
        mov [REACHED_PHY_MEM_ADDR_PAGINGTYPES], eax     ; Write down the result
    ret

;----------------------------------------------------------------------------------------------------------------------------------------



did_i_reach_last_pde:
    push eax                                                 ; Clear Carry
    mov eax, [REACHED_PHY_MEM_ADDR_PAGINGTYPES]         ; Did we reach the last PDPE?
    and eax, REACHED_LAST_PDE                         ; Last PDPE reached?
    cmp eax, REACHED_LAST_PDE                         ;
    jne .pde_not_set
    stc                                                 ; Reached last PDPE
    jmp .end

.pde_not_set:
    clc

.end:



    pop eax
    ret


reached_last_pde:
        mov eax, [REACHED_PHY_MEM_ADDR_PAGINGTYPES]     ; Write down the result
        or eax, REACHED_LAST_PDE                       ; Check global definitions.asm,  (Bit 3: Last PDPE)
        mov [REACHED_PHY_MEM_ADDR_PAGINGTYPES], eax     ; Write down the result
    ret


;----------------------------------------------------------------------------------------------------------------------------------------



did_i_reach_last_pte:
    push eax                                                 ; Clear Carry
    mov eax, [REACHED_PHY_MEM_ADDR_PAGINGTYPES]         ; Did we reach the last PDPE?
    and eax, REACHED_LAST_PTE                         ; Last PDPE reached?
    cmp eax, REACHED_LAST_PTE                         ;
    jne .pte_not_set
    stc                                                 ; Reached last PDPE
    jmp .end

.pte_not_set:
    clc

.end:



    pop eax
    ret


reached_last_pte:
        mov eax, [REACHED_PHY_MEM_ADDR_PAGINGTYPES]     ; Write down the result
        or eax, REACHED_LAST_PTE                       ; Check global definitions.asm,  (Bit 3: Last PDPE)
        mov [REACHED_PHY_MEM_ADDR_PAGINGTYPES], eax     ; Write down the result
    ret




[SECTION .data]

;paging_entry: dq 0
;pml4t: dd 0 ; PML4 Table location (this is just a place holder. Please define it in asm32_setup_64bits_paging_structures.inc)









