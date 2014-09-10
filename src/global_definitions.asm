
%ifndef GLOBAL_DEFS
%define GLOBAL_DEFS

;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Screen definitions.
%define LENGTH 80
%define HEIGHT 25
%define video_address 0xB8000
%define space_char 0x20

%define FIRST_BYTE_OF_LAST_LINE (LENGTH * HEIGHT * 2 - (LENGTH*2)) + video_address


;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; PIC Definitions.
                                                        ; 
%define PIC1_COMMAND 0x20                               ; Command port of the first PIC
%define PIC1_DATA    0x21                               ; Data/IMR (Interrupt Mask Register) port of the first PIC
                                                        ;
%define PIC2_COMMAND 0xA0                               ; Command port of the second PIC
%define PIC2_DATA    0xA1                               ; Data/IMR (Interrupt Mask Register) port of the second PIC
                                                        ;
%define PIC_EOI 0x20                                    ; End-of-interrupt command code
                                                        ;
                                                        ;
%define ICW1 00010001b                                  ; (0x11) Enables initialization mode and says we'll set ICW4
%define IRQ0 0x20                                       ; IRQ0 to be mapped to interrupt vector 0x20
%define IRQ8 0x28                                       ; IRQ8 to be mapped to interrupt vector 0x28
                                                        ;              
%define IDT32_BASE      0x7000                          ; IDT Location for 32 and 64 bits
%define IDT64_BASE      0x7000                          ; in 64 bits, the IDT will go to 0x7FFF
                                                        ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
%define MEMMAP_START    0x9000                          ; Where the memory map is stored in phy mem


[SECTION .data]

highest_mmap_entry:             dd 0 ; entry with the highest address
data_counter_mmap_entries:      dd 0 ; How many entries in the memmap?
mem_amount:                     dq 0 ; How much memory the system has?

GLOBAL_OS_COUNTER:              dq 0 ; See timer IRQ handling

LAST_PHY_MEM_ADDR_SIGN_EXTENT:  dd 0
LAST_PHY_MEM_ADDR_PML4E:        dd 0
LAST_PHY_MEM_ADDR_PDPE:         dd 0
LAST_PHY_MEM_ADDR_PDE:          dd 0
LAST_PHY_MEM_ADDR_PTE:          dd 0
LAST_PHY_MEM_ADDR_PAGE_OFFSET:  dd 0

REACHED_PHY_MEM_ADDR_PAGINGTYPES: dd 0  ; Bit 0: Unused, free to go :D
                                        ; Bit 1: Last PTE
                                        ; Bit 2: Last PDE
                                        ; Bit 3: Last PDPE
                                        ; Bit 4: Last PML4E

CURRENTLY_PAGED_ADDRESS dq 0 ; Used as a "counter" on which page base address the PTE will take the address it relates to.

%define REACHED_LAST_PML4E  10000b
%define REACHED_LAST_PDPE    1000b
%define REACHED_LAST_PDE      100b
%define REACHED_LAST_PTE       10b

%define MAX_ENTRIES_IN_PAGING_TABLE 511 ; In fact 512... 0 to 511


;current_LAST_PHY_MEM_ADDR_SIGN_EXTENT:  dd 0
;current_LAST_PHY_MEM_ADDR_PML4E:        dd 0
;current_LAST_PHY_MEM_ADDR_PDPE:         dd 0
;current_LAST_PHY_MEM_ADDR_PDE:          dd 0
;current_LAST_PHY_MEM_ADDR_PTE:          dd 0
;current_LAST_PHY_MEM_ADDR_PAGE_OFFSET:  dd 0


%endif


