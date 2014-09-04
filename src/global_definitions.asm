
%ifndef GLOBAL_DEFS
%define GLOBAL_DEFS



%define MEMMAP_START 0x8000 ; Where the memory map is stored in phy mem


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


