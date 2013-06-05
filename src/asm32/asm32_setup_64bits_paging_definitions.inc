
%ifndef PAGING_DEFINITIONS
%define PAGING_DEFINITIONS

[SECTION .data]



;MAX_ADDR        dq 0
;MAX_ADDR_LEN    dq 0 

                                                        ;
PML4T_LOCATION: dd 0                                     ; Where the PML4T is located. (Begining of paging structures)

PAGING_LOCATION_END: dd 0                               ; End of paging structures

PAGING_LOCATION_PDPE_CURRENTLY_WORKING_AT: dd 0              ; Current address on which the PDPE functions are working.
PAGING_LOCATION_PDE_CURRENTLY_WORKING_AT: dd 0              ; Current address on which the PDE functions are working.
PAGING_LOCATION_PTE_CURRENTLY_WORKING_AT: dd 0              ; Current address on which the PTE functions are working.

pml4e: dq 0
                                                        ; PML4E format:
                                                        ; - Bit  63       : NX, no execute
                                                        ; - Bits 62 to 52 : Available
                                                        ; - Bits 51 to 12 : PDPT base address
                                                        ; - Bits 11 to  9 : AVL                 ; Free for our O.S. to use
                                                        ; - Bits  8 to  7 : MBZ                 ; MBZ (Must Be Zero)
                                                        ; - Bit   6       : IGN                 ; 
                                                        ; - Bit   5       : A                   ; Accessed
                                                        ; - Bit   4       : PCD                 ; Page level cache disable (1), Cache pages (0)
                                                        ; - Bit   3       : PWT                 : Write cache (0), Write through (1)
                                                        ; - Bit   2       : U/S                 : User/Supervisor if 1 CPL3 can access if 0 only CPL0,1,2 can access
                                                        ; - Bit   1       : R/W                 : Read only or read write? (1=rw)
                                                        ; - Bit   0       : P                   : Present in memory, if at 0, Page fault (#PF)




pdpe: dq 0                                              ; PDPE, PDE, same format:
pde: dq 0                                               ; - Bit  63       : NX, no execute
                                                        ; - Bits 62 to 52 : Available
                                                        ; - Bits 51 to 12 : PDPT base address
                                                        ; - Bits 11 to  9 : AVL                 ; Free for our O.S. to use
                                                        ; - Bit   8       : MBZ                 ; MBZ (Must Be Zero)
                                                        ; - Bit   7       : MBZ                 ; MBZ (Must Be Zero)
                                                        ; - Bit   6       : IGN                 ; 
                                                        ; - Bit   5       : A                   ; Accessed
                                                        ; - Bit   4       : PCD                 ; Page level cache disable (1), Cache pages (0)
                                                        ; - Bit   3       : PWT                 : Write cache (0), Write through (1)
                                                        ; - Bit   2       : U/S                 : User/Supervisor if 1 CPL3 can access if 0 only CPL0,1,2 can access
                                                        ; - Bit   1       : R/W                 : Read only or read write? (1=rw)
                                                        ; - Bit   0       : P                   : Present in memory, if at 0, Page fault (#PF)





pte: dq 0                                                ; PTE
                                                        ; - Bit  63       : NX, no execute
                                                        ; - Bits 62 to 52 : Available
                                                        ; - Bits 51 to 12 : PDPT base address
                                                        ; - Bits 11 to  9 : AVL                 ; Free for our O.S. to use
                                                        ; - Bit   8       : G                   ; Global
                                                        ; - Bit   7       : PAT                 ; 
                                                        ; - Bit   6       : D                   ; 
                                                        ; - Bit   5       : A                   : 
                                                        ; - Bit   4       : PCD                 ; Page level cache disable (1), Cache pages (0)
                                                        ; - Bit   1       : R/W                 : Read only or read write? (1=rw)
                                                        ; - Bit   3       : PWT                 : Write cache (0), Write through (1)
                                                        ; - Bit   2       : U/S                 : User/Supervisor if 1 CPL3 can access if 0 only CPL0,1,2 can access
                                                        ; - Bit   1       : R/W                 : Read only or read write? (1=rw)
                                                        ; - Bit   0       : P                   : Present in memory, if at 0, Page fault (#PF)















%endif
