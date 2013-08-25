[BITS 32]
[SECTION .text]


%define PAGE_PRESENT                 1b                 ; - Present in memory
%define PAGE_WRITEABLE              10b                 ; - Read / Write
%define PAGE_NOT_PROTECTED         100b                 ; - Defeat protection, memory is accessible by CPL > 0
%define PAGE_WRITETHROUGH_CACHING 1000b                 ; - Use Write through caching




%include 'global_definitions.asm'
%include 'asm32/asm32_setup_64bits_paging_definitions.inc'
%include 'asm32/asm32_setup_64bits_paging_structures_lib.inc'

setup_64_bits_paging_structures:



;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Step 1, Calculate the necessary structures with the size of physical memory.
                                                        ;
    mov ebx, mem_amount                                 ; Physical memory size (Last address) location
                                                        ;
                                                        ;xor eax, eax
                                                        ;mov DWORD [ebx+4], eax
                                                        ;mov DWORD [ebx], eax
                                                        ;
                                                        ;mov DWORD [ebx], 0x7F
                                                        ;mov DWORD [ebx+4], 0xC0000000
                                                        ;
                                                        ; Offset 0000000000000000 000000000 000000000 000000000 000000000 111111111111 OK          (0xFFF)
                                                        ; PTE    0000000000000000 000000000 000000000 000000000 111111111 000000000000 OK       (0x1FF000)
                                                        ; PDE    0000000000000000 000000000 000000000 111111111 000000000 000000000000 OK     (0x3FE00000)
                                                        ; PDPE   0000000000000000 000000000 111111111 000000000 000000000 000000000000 OK   (0x7F 0xC0000000)
                                                        ; PML4E  0000000000000000 111111111 000000000 000000000 000000000 000000000000 OK (0xFF80 00000000)
                                                        ; Extent 1111111111111111 000000000 000000000 000000000 000000000 000000000000
                                                        ;                             Higher Half  |  Lower Half
                                                        ;                                        bit 31
                                                        ;
                                                        ;mov eax, 0xFFFFFFFF
                                                        ;mov [ebx], eax
                                                        ;mov [ebx+4], eax
                                                        ;
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; Calc offset
    mov eax, [ebx+4]                                    ;
    and eax, 0xFFF                                      ; 12 lower bits
    mov [LAST_PHY_MEM_ADDR_PAGE_OFFSET], eax            ;
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; Calc Last PTE
    mov eax, [ebx+4]                                    ;
    shr eax, 12                                         ;
    and eax, 0x1FF                                      ; 9 bits of the PTE
    mov [LAST_PHY_MEM_ADDR_PTE], eax                    ;
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; Calc Last PDE
    mov eax, [ebx+4]                                    ;
    shr eax, 21                                         ;
    and eax, 0x1FF                                      ; 9 bits of the PDE
    mov [LAST_PHY_MEM_ADDR_PDE], eax                    ;
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; Calc Last PDPE
    mov eax, [ebx]                                      ;
    and eax, 1111111b                                   ;
    shl eax, 2                                          ;
                                                        ;
    mov edx, [ebx+4]                                    ;
    shr edx, 30                                         ;
                                                        ;
    or eax, edx                                         ;
                                                        ;
    and eax, 0x1FF                                      ; 9 bits of the PDPE
    mov [LAST_PHY_MEM_ADDR_PDPE], eax                   ;
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; Calc Last PML4E
    mov eax, [ebx]                                      ;
    shr eax, 7                                          ;
    and eax, 0x1FF                                      ; 9 bits of the PTE
    mov [LAST_PHY_MEM_ADDR_PML4E], eax                  ;
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; Register global variables.
    mov dword [PML4T_LOCATION], 0x100000                 ;
    mov eax, [PML4T_LOCATION]                           ;
    mov dword [PAGING_LOCATION_END], eax                ;
                                                        ;
                                                        ;--------------------------------------------------------------------------------

    ; First, calculate where we'll put the paging data structures.
    ; For this, we'll find the free area above 0xFFFF.

;mov ebx, [LAST_PHY_MEM_ADDR_PDPE]                                      ; Load kernel end address.
;call asm32_display_make_string_from_hex                 ; Make is hex string
;mov esi, ebx                                            ;
;call asm32_display_writestring            ;

    call setup_64_bits_paging_structures_pml4


;cli
;hlt

;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Step 2, create the PML4T
                                                        ;
                                                        ; Now create paging structures.
    mov ecx, [LAST_PHY_MEM_ADDR_PML4E]                  ; Until the last PML4E for the physical memory address
    mov edx, 0                                          ; Current PML4E treated
                                                        ;
    mov ebx, [PML4T_LOCATION]                           ; Where's our PML4T?
                                                        ;
                                                        ;--------------------------------------------------------------------------------
    .next_pml4e:                                        ; Treatment of each PML4E starts here.

        cmp edx, ecx                                    ;
        jne .not_last_pml4e                             ; Not the last pml4e? jump!

        call reached_last_pml4e                                                        ;

    .not_last_pml4e:                                    ;

                                                        ;
        push eax                                        ;
        push ecx                                        ;
        push edx                                        ;
        push ebx                                                ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; Will store the PML4E in ecx:edx
        xor ecx, ecx                                    ; Zero Both
        xor edx, edx                                    ;
                                                        ;
        mov edx, [PAGING_LOCATION_END]                  ; Location for the new PDPT
                                                        ;
                                                        ;
        ;mov edx, eax                                    ; Set the memory address of the PDPT in the PML4E, episode 2
                                                        ; PML4E flags:
        or edx, PAGE_PRESENT | PAGE_WRITEABLE | PAGE_NOT_PROTECTED | PAGE_WRITETHROUGH_CACHING
                                                        ;
;mov ecx, 'PML4'
;mov ecx, 'E   '
        mov [ebx+4], ecx                                ; Writing the entry in memory
        mov [ebx], edx                                  ; Bis repetita
                                                        ;
                                                        ; Done, PML4E is done.
                                                        ;--------------------------------------------------------------------------------
        call create_pdpt                                ; We now need to create our PDPT pointed by the PML4E
                                                        ;
                                                        ;--------------------------------------------------------------------------------
        pop ebx
        pop edx                                         ; Done for this PML4E!
        pop ecx                                         ;
        pop eax                                         ;
                                                        ; ∴
        add ebx, 8                                      ; Will point to the next entry of the PML4T
                                                        ; Add 8 bytes to base address, each PML4E is 64 bits wide
        inc edx                                         ; Current entry number (not address)
                                                        ;
        cmp edx, ecx                                    ; Is the current entry below the max entry?
        jbe .next_pml4e                                 ; Treat the next entry
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; Last entry treated. Begone!
                                                        ;
    ret                                                 ; Paging structures are now set up.
                                                        ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
create_pdpt:                                            ;
    mov eax, [PAGING_LOCATION_END]                      ; Next Free zone!
    mov [PAGING_LOCATION_PDPE_CURRENTLY_WORKING_AT], eax ; We now will be working on this part:
                                                        ;
                                                        ;
    push dword [PAGING_LOCATION_END]                    ; Zero the PDPT area PAGING_LOCATION_END will be updated to after this zone.
    call create_empty_table                             ; 
                                                        ;--------------------------------------------------------------------------------
                                                        ; We need to calculate to know if this will be using 512 PDPE or less (Last PML4E)
                                                        ;
    mov ecx, MAX_ENTRIES_IN_PAGING_TABLE                ; By default, we should have 512 PDPE
                                                        ;
    call did_i_reach_last_pml4e
    jnc .done_calc_num_pdpe                             ; Not last pml4e? jump!
                                                        ; Last PML4E, we'll probably have a limited number of PDPE
    mov ecx, [LAST_PHY_MEM_ADDR_PDPE]                   ; Which PDPE will be the last?
                                                        ;
    .done_calc_num_pdpe:                                ; Done!
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ;
                                                        ;
    mov edx, 0                                          ; Current PDPE
                                                        ;
    mov ebx, [PAGING_LOCATION_PDPE_CURRENTLY_WORKING_AT] ; Write the PDPE at this free space.
                                                        ;
    .next_pdpe:                                         ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; Indicate we're at the last PDPE... Well, if we are!
                                                        ;
                                                        ; I KNOW THIS IS TEMPTING BUT....
                                                        ; THIS CANNOT BE MOVED AT THE END OF THE FUNCTION OR THE NEXT PDT WON'T
                                                        ; BE AWARE WE'RE ON THE LAST PDPE!!!!!
                                                        ;
        push eax                                        ; Save eax
                                                        ;

        call did_i_reach_last_pml4e                     ; Last PML4E? If so, we need to check if we're at last PDPE
        jnc .not_last_pdpe                              ;
                                                        ;
        cmp edx, ecx                                    ; Last PDPE?
        jne .not_last_pdpe                              ;
                                                        ;
        call reached_last_pdpe                          ; Tell last PDPE has now been reached

    .not_last_pdpe:                                     ;
                                                        ;--------------------------------------------------------------------------------
        pop eax                                         ; Restore eax





        push eax                                        ;
        push ecx                                        ; Save some registers
        push edx                                        ;
        push ebx
                                                        ;
                                                        ; Will store the PML4E in ecx:edx
        xor ecx, ecx                                    ; Zero Both
        xor edx, edx                                    ;
                                                        ;
        mov eax, [PAGING_LOCATION_END]                  ; Location for the new PDT
                                                        ;
                                                        ;
        mov edx, eax                                    ; Set the memory address of the PDT, part 2
                                                        ;
        or edx, PAGE_PRESENT | PAGE_WRITEABLE | PAGE_NOT_PROTECTED | PAGE_WRITETHROUGH_CACHING
                                                        ;
                                                        ; Now that our 64 bits PDPE is built, let's write it in the PDPT
;mov ecx, 'PDPE'
;mov ecx, 'XXXX'
                                                        ;
        mov [ebx+4], ecx                                ; Write the last part of the PDPE
        mov [ebx], edx                                  ; Write the first part of the PDPE
                                                        ;
        ; Done, PML4E is done.
;call display_reached_paging_types                                                        ;
;call display_reached_paging_types

         call create_pdt                                 ; Create the PDT pointed by the PDPE now.
                                                        ;
        pop ebx
        pop edx                                         ; Restore used registers
        pop ecx                                         ;
        pop eax                                         ;
                                                        ; ∴
        add ebx, 8                                     ;  Add 8 bytes to base address, each PDPE is 64 bits wide
        inc edx                                         ;


        cmp edx, ecx                                    ;
        jbe .next_pdpe                                  ;
                                                        ;
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ;
    ret                                                 ; PDPT is done!
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
create_pdt:                                            ;
    mov eax, [PAGING_LOCATION_END]                      ; Next Free zone!
    mov [PAGING_LOCATION_PDE_CURRENTLY_WORKING_AT], eax ; We now will be working on this part:
                                                        ;
                                                        ;
    push dword [PAGING_LOCATION_END]                    ; Zero the PDT area PAGING_LOCATION_END will be updated to after this zone.
    call create_empty_table                             ; 
                                                        ;--------------------------------------------------------------------------------
                                                        ; We need to calculate to know if this will be using 512 PDE or less (Last PDPE)
                                                        ;
    mov ecx, MAX_ENTRIES_IN_PAGING_TABLE                ; By default, we should have 512 PDE
                                                        ;
    call did_i_reach_last_pdpe
    jnc .done_calc_num_pde                              ; Not last pdpe? jump!
                                                        ; Last PDPE, we'll probably have a limited number of PDE
    mov ecx, [LAST_PHY_MEM_ADDR_PDE]                    ; Which PDE will be the last?

                                                        ;
.done_calc_num_pde:                                     ; Done!
                                                        ;


                                                        ;--------------------------------------------------------------------------------
                                                        ;
                                                        ;
    mov edx, 0                                          ; Current PDE
                                                        ;
    mov ebx, [PAGING_LOCATION_PDE_CURRENTLY_WORKING_AT] ; Write the PDE at this free space.
                                                        ;
    .next_pde:                                          ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; Indicate we're at the last PDE... Well, if we are!
        push eax                                        ; Save eax
                                                        ;
        call did_i_reach_last_pdpe                      ;
        jnc .not_last_pde                              ;
                                                        ;
        cmp edx, ecx                                    ; Last PDE?
        jne .not_last_pde                              ;

        call reached_last_pde

    .not_last_pde:
        pop eax                                         ; Restore eax
                                                        ;--------------------------------------------------------------------------------

        push eax                                        ;
        push ecx                                        ; Save some registers
        push edx                                        ;
        push ebx                                                        ;
                                                        ; Will store the PML4E in ecx:edx
        xor ecx, ecx                                    ; Zero Both
        xor edx, edx                                    ;
                                                        ;
        mov eax, [PAGING_LOCATION_END]                  ; Location for the new PTT
                                                        ;
        mov edx, eax                                    ; Set the memory address of the PTT, part 2
        or edx, PAGE_PRESENT | PAGE_WRITEABLE | PAGE_NOT_PROTECTED | PAGE_WRITETHROUGH_CACHING
                                                        ;
                                                        ; Now that our 64 bits PDE is built, let's write it in the PDT
;mov ecx, 'PDEX'
;mov ecx, 'XXXX'
                                                        ;
        mov [ebx+4], ecx                                ; Write the last part of the PDE
        mov [ebx], edx                                  ; Write the first part of the PDE
                                                        ;
        ; Done, PDE is done.
                                                        ;
        call create_ptt                                 ; Create the PTT pointed by the PDE now.
        pop ebx                                                ;
        pop edx                                         ; Restore used registers
        pop ecx                                         ;
        pop eax                                         ;
                                                        ; ∴
        add ebx, 8                                     ;  Add 8 bytes to base address, each PDE is 64 bits wide
        inc edx                                         ;

;call display_current_page_base_addr



        cmp edx, ecx                                    ;
        jbe .next_pde                                  ;
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ;
    ret                                                 ; PDT is done!
                                                        ;






;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------

create_ptt:                                            ;
    mov eax, [PAGING_LOCATION_END]                      ; Next Free zone!
    mov [PAGING_LOCATION_PTE_CURRENTLY_WORKING_AT], eax ; We now will be working on this part:
                                                        ;
                                                        ;
    push dword [PAGING_LOCATION_END]                    ; Zero the PDT area PAGING_LOCATION_END will be updated to after this zone.
    call create_empty_table                             ; 
                                                        ;--------------------------------------------------------------------------------
                                                        ; We need to calculate to know if this will be using 512 PTE or less (Last PDE)
                                                        ;
    mov ecx, MAX_ENTRIES_IN_PAGING_TABLE                ; By default, we should have 512 PTE
                                                        ;
    call did_i_reach_last_pde
    jnc .done_calc_num_pte                              ; Not last pde? jump!
                                                        ; Last PDE, we'll probably have a limited number of PTE
    mov ecx, [LAST_PHY_MEM_ADDR_PTE]                    ; Which PTE will be the last?

                                                        ;
.done_calc_num_pte:                                     ; Done!
                                                        ;


                                                        ;--------------------------------------------------------------------------------
                                                        ;
                                                        ;
    mov edx, 0                                          ; Current PTE
                                                        ;
    mov ebx, [PAGING_LOCATION_PTE_CURRENTLY_WORKING_AT] ; Write the PTE at this free space.
                                                        ;
    .next_pte:                                          ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; Indicate we're at the last PTE... Well, if we are!
        push eax                                        ; Save eax
                                                        ;
        call did_i_reach_last_pde                      ;
        jnc .not_last_pte                              ;
                                                        ;
        cmp edx, ecx                                    ; Last PDE?
        jne .not_last_pte                              ;

        call reached_last_pte

    .not_last_pte:
        pop eax                                         ; Restore eax
                                                        ;--------------------------------------------------------------------------------

        push eax                                        ;
        push ecx                                        ; Save some registers
        push edx                                        ;
                                                        ;
                                                        ; Will store the PTE in ecx:edx
        xor ecx, ecx                                    ; Zero Both
        xor edx, edx                                    ;
                                                        ;
        mov eax, [CURRENTLY_PAGED_ADDRESS]                  ; Location for the physical page
                                                        ;
        mov edx, eax                                    ; Set the memory address of the physical page, part 2
        shl edx, 12                                     ;
                                                        ;
        or edx, PAGE_PRESENT | PAGE_WRITEABLE | PAGE_NOT_PROTECTED | PAGE_WRITETHROUGH_CACHING

                                                        ; Now that our 64 bits PTE is built, let's write it in the PTT
;mov ecx, 'PTEX'
;mov ecx, 'XXXX'
                                                        ;
        mov [ebx+4], ecx                                ; Write the last part of the PTE
        mov [ebx], edx                                  ; Write the first part of the PTE
                                                        ;
                                                        ;
        ; Nope, we have all paging structures now, nothing more :P
                                                        ;
        pop edx                                         ; Restore used registers
        pop ecx                                         ;
        pop eax                                         ;
                                                        ; ∴
        add ebx, 8                                     ;  Add 8 bytes to base address, each PTE is 64 bits wide
        inc edx                                         ;

        push eax
        mov eax, [CURRENTLY_PAGED_ADDRESS]
        inc eax ; One page
        mov [CURRENTLY_PAGED_ADDRESS], eax
        pop eax
;call display_current_page_base_addr

;call display_reached_paging_types                                                        ;


        cmp edx, ecx                                    ;
        jbe .next_pte                                  ;
                                                        ;
;call display_reached_paging_types                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ;
    ret                                                 ; PTT is done!
                                                        ;





[SECTION .data]












;mov ebx, [LAST_PHY_MEM_ADDR_PML4E]                                      ; Load kernel end address.
;call asm32_display_make_string_from_hex                 ; Make is hex string
;;mov esi, ebx                                            ;
;call asm32_display_writestring            ;




;cli
;hlt

                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Step 3, Set the higher half kernel entries.

                                                        ; Will start to map the first 1MB (from 0x0 to 0xFFFF at higher half of usable virtual memory)
                                                        ; Address: 0xFFFF800000000000
                                                        ; Binary:  1111111111111111100000000000000000000000000000000000000000000000
                                                        ;
                                                        ; SIGN_EXTENT: FFFF
                                                        ; PML4E: 100
                                                        ; PDPE: 0
                                                        ; PDE: 0
                                                        ; PTE: 0
                                                        ; PHYSICAL_PAGE_OFFSET: 0
                                                        ;
                                                        ; SIGN_EXTENT: 00000000000000000000000000000000000000000000000000000000000000001111111111111111
                                                        ; PML4E: 100000000
                                                        ; PDPE: 000000000
                                                        ; PDE: 000000000
                                                        ; PTE: 000000000
                                                        ; PHYSICAL_PAGE_OFFSET: 000000000000

    






    ; Step 4, map from 0x0000,0000,0000,0000 to whatever the remaining memory.


    ; Paging structures should now be initialized.
    ; Next step, enable EFER.LME and paging, check EFER.LMA

















;mov ebx, [LAST_PHY_MEM_ADDR_PML4E]                                      ; Load kernel end address.
;call asm32_display_make_string_from_hex                 ; Make is hex string
;mov esi, ebx                                            ;
;call asm32_display_writestring            ;

;mov ebx, [LAST_PHY_MEM_ADDR_PDPE]                                      ; Load kernel end address.
;call asm32_display_make_string_from_hex                 ; Make is hex string
;mov esi, ebx                                            ;
;call asm32_display_writestring            ;

;mov ebx, [LAST_PHY_MEM_ADDR_PDE]                                      ; Load kernel end address.
;call asm32_display_make_string_from_hex                 ; Make is hex string
;mov esi, ebx                                            ;
;call asm32_display_writestring            ;

;mov ebx, [LAST_PHY_MEM_ADDR_PTE]                                      ; Load kernel end address.
;call asm32_display_make_string_from_hex                 ; Make is hex string
;mov esi, ebx                                            ;
;call asm32_display_writestring            ;

;mov ebx, [LAST_PHY_MEM_ADDR_PAGE_OFFSET]                                      ; Load kernel end address.
;call asm32_display_make_string_from_hex                 ; Make is hex string
;mov esi, ebx                                            ;
;call asm32_display_writestring            ;







;    mov eax, [PAGING_LOCATION_END]
;    add eax, 4096
;    mov [PAGING_LOCATION_END], eax

