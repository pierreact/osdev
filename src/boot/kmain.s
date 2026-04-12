
;
; O.S. Still without a name... This file has the kernel code...
; Copyright (C) 2012 - 2014 Pierre ANCELOT ∴
; 
; This program is free software; you can redistribute it and/or
; modify it under the terms of the GNU General Public License
; as published by the Free Software Foundation; either version 2
; of the License, or (at your option) any later version.
; 
; This program is distributed in the hope that it will be useful,
; but WITHOUT ANY WARRANTY; without even the implied warranty of
; MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
; GNU General Public License for more details.
; 
; You should have received a copy of the GNU General Public License
; along with this program; if not, write to the Free Software
; Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
;
; You can reach the author by sending a mail to pierreact at gmail dot com
; Thanks :P
;
; QEMU DEBUG:
; stop
; pmemsave 0 134217728 qemu_mem_dump.bin
; info registers 
; info mem

;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
%include "global_definitions.asm"                       ; Definitions
                                                        ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
[BITS 16]                                               ;
[SECTION .text]                                         ;
                                                        ;
GLOBAL GDT64
EXTERN MEMMAP_START                                     ; defined as absolute symbol in link.ld
GLOBAL kmain                                            ;--------------------------------------------------------------------------------
kmain:                                                  ; Starting point of the kernel called by the bootsector
                                                        ;
EXTERN kernelEnd, __code, __data, __bss                 ; External symbols.
EXTERN shell_init                                       ; Shell initialization function.
EXTERN heap_init                                        ; Heap initialization.
EXTERN ide_init                                         ; IDE initialization.
EXTERN fat32_init                                       ; FAT32 initialization.
EXTERN acpi_init                                        ; ACPI RSDP/MADT parsing.
EXTERN map_mmio_region                                  ; Map APIC MMIO pages.
EXTERN disable_pic                                      ; Disable 8259A PIC.
EXTERN lapic_init                                       ; Initialize Local APIC on BSP.
EXTERN ioapic_init                                      ; Initialize I/O APIC.
EXTERN cpu_init                                         ; Initialize per-CPU structures.
EXTERN tss_init                                         ; Initialize TSS and load TR.
EXTERN percpu                                           ; Per-CPU state array (kernel/cpu.c).
EXTERN task_init                                        ; Initialize BSP task scheduler.
EXTERN task_create                                      ; Create a BSP ring 3 task.
EXTERN task_run_first                                   ; Start first BSP task (drops to ring 3).
EXTERN shell_ring3_entry                                ; Shell entry point for ring 3.
EXTERN ap_startup                                       ; Wake Application Processors.
EXTERN pci_init                                         ; Enumerate PCI devices.
EXTERN ahci_init                                        ; Initialize AHCI SATA controller.
EXTERN bootmedia_init                                   ; Discover and mount boot ISO.
EXTERN nic_init                                         ; Initialize NIC drivers.
                                                        ;
jmp end_define_functions                                ; Including 16 bits functions
%include "asm16/asm16_display.inc"                      ; Screen functions to display stuff on screen
%include "asm16/asm16_interrupts.inc"                   ; Setup Interrupt Vector Table and defined ISRs
%include "asm16/asm16_get_memmap.inc"                   ; Get memory map of the system.
[SECTION .text]                                         ; Ensure we're back in .text
end_define_functions:                                   ; Done.
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Setting up segments and stack.
                                                        ;
cli					                                    ; Clear interrupts before changing the stack
mov ax, 0                                               ;
mov ds, ax                                              ; data and extra segments at 0. Labels return the full address already.
mov es, ax                                              ;
mov ax, 0x8000		                                    ; stack at 0x8F000
mov ss, ax                                              ;
mov sp, 0xf000                                          ;
                                                        ; NOTE: Keep interrupts disabled until IVT is set up
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Switch to 80x50 text mode (load 8x8 font)
mov ax, 0x1111                                          ; AH=11h (char gen), AL=11h (load 8x14 font)
mov bl, 0                                               ; Block 0
int 0x10                                                ;
call asm16_display_clear                                ; Clear screen after font change
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Setting up cursor blinking frequency.
mov ah, 1                                               ;
mov cx, 00010000b                                       ;
int 0x10                                                ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
                                                        ; In order for the firmware built into the system to optimize itself
                                                        ; for running in Long Mode, AMD recommends that the OS notify the BIOS 
                                                        ; about the intended target environment that the OS will be running in: 
                                                        ; - 32-bit mode
                                                        ; - 64-bit mode
mov si, msg16_tellbios64                                ; - A mixture of both modes
call asm16_display_writestring                          ;
                                                        ; This can be done by calling the BIOS interrupt 15h from Real Mode with 
mov ax, 0xEC00                                          ; AX set to 0xEC00
mov bl, 2                                               ; BL set to:
                                                        ; - 1 for 32-bit Protected Mode
                                                        ; - 2 for 64-bit Long Mode
                                                        ; - 3 if both modes will be used.
int 0x15                                                ;
                                                        ; Right, this OS will only run 64 bits applications, no 32 bits.
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
mov ax, MEMMAP_START
shr ax, 4
mov es, ax
mov di, 0
                                                        ;
call do_e820                                            ; Detect the amount of memory in the system and unusable memory zones.
                                                        ;
jnc memmap_ok                                           ; No error? 
                                                        ;
mov si, msg16_nommap                                    ; We rely on the memory map, if it's not supported by the BIOS, we die.
call asm16_display_writestring                          ; Call display function
cli ;LEAVE THIS                                         ; Disable interrupts
hlt ;LEAVE THIS                                         ; Die.
;################                                       ;
                                                        ;
memmap_ok:                                              ;
mov [data_counter_mmap_entries], ax                     ; Now the memory map is stored at 0x8000 (linear, not seg:offset) and has $data_counter_mmap_entries of 24 bytes each
                                                        ;
mov si, msg16_mmap_ok                                   ; Memory map is loaded.
call asm16_display_writestring                          ; Call display function
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
                                                        ; All interrupts, software or hardware point to the same function.
                                                        ; The function halts the O.S. and panics.
                                                        ;
call asm16_IVT_setup                                    ; Setting up the interrupt vector table, 16 bits equivalent of the IDT.
                                                        ;
                                                        ; NOTE: We DON'T enable interrupts here because:
                                                        ; 1. Hardware interrupts (timer, keyboard) would trigger our panic handler
                                                        ; 2. Software interrupts (int 0xFF below) work even with interrupts disabled
                                                        ; 3. We'll properly handle interrupts in 32/64-bit mode with the PIC/APIC
;int 0x30                                               ; This is a test, Kills the O.S., triggers an interrupt, and we don't accept them in this mode (other than 0xFF).
                                                        ;
int 0xFF                                                ; Used as a test for 16 bits IVT init. 
                                                        ; This is the only interrupt in 16 bits mode that won't kill the O.S.
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Init the GDT to jump in 32 bits mode.
                                                        ;
                                                        ; Show msg saying we are now going to load the 32 bits GDT
mov si, msg32gdt                                        ; Load message address in si
call asm16_display_writestring                          ; Call display function
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; Setup of the GDT means you need the GDT address and it's size
                                                        ; 
mov ax, gdtend                                          ; Calculate GDT size (gdtend - gdt)
mov bx, gdt                                             ;
sub ax, bx                                              ;
mov word [gdtptr], ax                                   ; Sets gdtptr limit.
                                                        ;
xor eax, eax                                            ; calculate GDT address
xor ebx, ebx                                            ;
                                                        ;
                                                        ; As of now, ds = 0 (even though it is actually loaded at 0x5000)
                                                        ; The label however provides us with the full address, 0x500(0) + offset.
mov ax, ds                                              ;
mov ecx, eax                                            ;
shl ecx, 4                                              ; Still is 0
                                                        ;
mov bx, gdt                                             ; Address of gdt
add ecx, ebx                                            ;
mov dword [gdtptr+2], ecx                               ; Sets gtrptr base
                                                        ;
                                                        ; This is the correct way to do it, however, in our case.
                                                        ; mov dword [gdtptr+2], gdt          would have worked.
                                                        ; 
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ; We now inform the processor of the location of the GDT in memory.
cli                                                     ; Disable interrupts
cld                                                     ; Clear direction flag
lgdt [gdtptr]                                           ; Loads GDT   
                                                        ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Switching to protected mode.
                                                        ; We will not however setup 32 bits paging as it's only a step for us to enable long mode.
                                                        ; This O.S. doesn't work on 32 bits! :)
                                                        ;
mov si, msgprot                                         ; show msg
call asm16_display_writestring                          ;
                                                        ;
                                                        ;--------------------------------------------------------------------------------
                                                        ;
    mov eax, cr0                                        ;
    or  ax, 1                                           ;
    mov cr0, eax                                        ; PE set to 1 (CR0)
                                                        ;
    jmp next                                            ;
next:                                                   ;
                                                        ;
    mov ax, 0x10                                        ; DS = FS = GS = ES = SS. 
    mov ds, ax                                          ; DS = 0x10, third GDT Entry
    mov fs, ax                                          ;
    mov gs, ax                                          ;
    mov es, ax                                          ;
    mov ss, ax                                          ;
    mov ebp, temp_stack_end                                ; Temporary stack (small, in BSS)
    jmp dword 0x8:kernel32                              ; reinit CS, CS = 0x8 (Second entry in GDT.)
                                                        ;
                                                        ;
[BITS 32]                                               ; Oh my....
section .text                                           ;
                                                        ;
kernel32:                                               ; Yay!
                                                        ;
                                                        ;
jmp include_32bits_functions                            ; Jump after functions inclusion
                                                        ;
%include 'asm32/asm32_display.inc'                      ; Screen functions.
%include 'asm32/asm32_cpuid_apic_support.inc'           ; Setup IDT + ISRs definitions
%include 'asm32/asm32_interrupts.inc'                   ; Setup IDT + ISRs definitions
%include 'asm32/asm32_pit.inc'                          ; Setup Programmable interrupt timer
%include 'asm32/asm32_cpuid_long_mode.inc'              ; Is our CPU able to activate long mode?
%include 'asm32/asm32_display_memory_map.inc'           ; Memory map display functions.
%include 'asm32/asm32_setup_64bits_paging_structures.inc' ; Paging structures setup for 64 bits mode.
                                                        ;
include_32bits_functions:                               ; 32 bits functions included.
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Do our CPU support APIC? If no, we die here, we don't support 8259A PIC in the long run.
mov eax, 1                                              ; Query CPU about it's features
cpuid                                                   ; Wee!
                                                        ;
and edx, 0x100                                          ; Test bit 9, APIC
cmp edx, 0x100                                          ; Is the bit set?
je asm32_apic_supported                                 ; If so, jump to APIC initialization
                                                        ;
                                                        ;
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
mov esi, msg_32_noapic                                  ; Load message informing apic ain't supported
call asm32_display_writestring                          ; Print it
cli ;LEAVE THIS                                         ; Disable interrupts
hlt ;LEAVE THIS                                         ; Die.
;################                                       ;
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
asm32_apic_supported:                                   ; APIC is supported, we can init it.
                                                        ;
mov esi, msg_32_apic                                    ; Load message informing apic is supported
call asm32_display_writestring                          ; Print it
                                                        ;
                                                        ;
;call asm32_cpuid_apic_init                              ; No APIC for now. Remember to setup APIC, 8259A has to be turned off.
                                                        ; This would be done using 
                                                        ; mov al, 0xff
                                                        ; out 0xa1, al
                                                        ; out 0x21, al
;----------------------------------------------------------------------------------------------------------------------------------------
call asm32_setup_pic                                    ; Seting up programmable Interrupt Controller 82C59A
;----------------------------------------------------------------------------------------------------------------------------------------
call asm32_setup_idt                                    ; Setup 32 bits IDT
;----------------------------------------------------------------------------------------------------------------------------------------
call asm32_setup_pit                                    ; Setup the programmable interrupt timer.
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
int 0xFE                                                ; Call an interrupt to prove the IDT is properly setup.
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Displaying some infos about the kernel
                                                        ;
mov esi, msg_32_bits                                    ; Now 32 bits
call asm32_display_writestring                          ;
                                                        ;
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
mov esi, msg_32_kernel_code                             ; .text location
call asm32_display_writestring                          ;
                                                        ;
mov ebx, __code                                         ; Load .text address
call asm32_display_make_string_from_hex                 ; Make it an hex string
mov esi, ebx                                            ; ...and...
call asm32_display_writestring_noscrollfirst            ; Print it.
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
mov esi, msg_32_kernel_data                             ; .data location
call asm32_display_writestring                          ;
                                                        ;
mov ebx, __data                                         ; Load .data address
call asm32_display_make_string_from_hex                 ; Make it an hex string
mov esi, ebx                                            ;
call asm32_display_writestring_noscrollfirst            ; Print it.
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
mov esi, msg_32_kernel_bss                              ; .bss location
call asm32_display_writestring                          ;
                                                        ;
mov ebx, __bss                                          ; Load .bss location
call asm32_display_make_string_from_hex                 ; Make it an hex string
mov esi, ebx                                            ;
call asm32_display_writestring_noscrollfirst            ; Print it.
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
mov esi, msg_32_kernel_end                              ; Location of where the kernel ends
call asm32_display_writestring                          ;
                                                        ;
mov ebx, kernelEnd                                      ; Load kernel end address.
call asm32_display_make_string_from_hex                 ; Make is hex string
mov esi, ebx                                            ;
call asm32_display_writestring_noscrollfirst            ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Enabling a20 line of the address bus, providing access to the whole memory
                                                        ; No, I don't test it before, I simply set it.
                                                        ; Why wasting time and cycles to test something I will ANYWAY set?
in al, 0x92                                             ; Load value of port 0x92 in al
or al, 2                                                ; set bit 2
out 0x92, al                                            ; Place al back in port 0x92
                                                        ;
mov esi, msg_a20_enabled                                ; Load message telling a20 is enabled
call asm32_display_writestring                          ; print it.
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
                                                        ; Can my CPU enable long mode?
push eax                                                ;
push edx                                                ;
                                                        ;
call asm32_cpuid_long_mode_support                      ; Defined in asm32/asm32_cpuid_long_mode.inc
xor edx, edx                                            ; edx = 0
cmp eax, edx                                            ; ?
ja cpu64_bits_supported                                 ; Jump if 64 bits supported
                                                        ;
mov esi, msg_check_cpu_64_no                            ; CPU has no support for 64 bits.
call asm32_display_writestring                          ; We say it.
                                                        ;
cli ;LEAVE THIS                                         ; Disable interrupts
hlt ;LEAVE THIS                                         ; Die, 64 bits not supported by processor.
;################                                       ;
                                                        ;
cpu64_bits_supported:                                   ; 64 bits supported!
mov esi, msg_check_cpu_64_yes                           ; We say it.
call asm32_display_writestring                          ;
                                                        ;
pop edx                                                 ; Restore registers
pop eax                                                 ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; 
                                                        ; Display memory map the map is made of a number of 24 bytes entries
push dword MEMMAP_START                                 ; 0x8000
push dword [data_counter_mmap_entries]                  ; 
                                                        ; Display memory map the map is made of a number of 24 bytes entries
call asm32_display_memmap                               ;
                                                        ;
push dword MEMMAP_START                                 ; 0x8000
push dword [data_counter_mmap_entries]                  ; 

call asm32_memmap_find_highest_entry

call asm32_get_last_mem_address                         ; Test VM reports us at 0x1,0000,0000 (4GB)

                                                        ; $ bash linear_address_to_paging.sh 0x100000000
                                                        ; Address: 0000000000000000100000000
                                                        ; Binary:  100000000000000000000000000000000
                                                        ; 
                                                        ; SIGN_EXTENT: 0
                                                        ; PML4E: 0
                                                        ; PDPE: 4
                                                        ; PDE: 0
                                                        ; PTE: 0
                                                        ; PHYSICAL_PAGE_OFFSET: 0
                                                        ; 
                                                        ; SIGN_EXTENT: 0000000000000000000000000000000000000000000000000
                                                        ; PML4E: 000000000
                                                        ; PDPE: 000000100
                                                        ; PDE: 000000000
                                                        ; PTE: 000000000
                                                        ; PHYSICAL_PAGE_OFFSET: 000000000000
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; Initializing 64 bits data structures before jumping there.
                                                        ;
                                                        ; Now we calc the Descriptors of the address of the last paging entry for the pmem
                                                        ;--------------------------------------------------------------------------------
                                                        ; PML4E
                                                        ; PDPE
                                                        ; PDE
                                                        ; - Bits 63-48 are a sign extension of bit 47, as required for canonical-address forms.
                                                        ; - Bits 47-39 index into the 512-entry page-map level-4 table.
                                                        ; - Bits 38-30 index into the 512-entry page-directory pointer table.
call setup_64_bits_paging_structures                    ; - Bits 29-21 index into the 512-entry page-directory table.
                                                        ; - Bits 20-12 index into the 512-entry page table.
                                                        ; - Bits 11-0 provide the byte offset into the physical page.
                                                        ;
                                                        ; CR3 points to PML4 which has a max of 512 entries of type PML4E(ntries).
                                                        ; Each PML4E point in turn to a PDP, which has a max of 512 PDPE(ntries).
                                                        ; Each PDPE point in turn to a PD made of a max of 512 PDE(ntries).
                                                        ; Each PDE point in turn to a PT made of a max of 512 PTE(ntries).
                                                        ; Each PTE point to a physical 4KB page, which when complete to the offset make the address.
                                                        ; A virtual address is therefore made of:
                                                        ; Sign extend + PML4E + PDPE + PDE + PTE + Offset.
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
                                                        ; ====================
                                                        ; Enable long mode in 64 bit mode with 4kB pages.
                                                        ; We therefore should have:
                                                        ;
                                                        ; CR0.PG=0  (Paging Disable, just to ensure)
                                                        ; CR4.PAE=1 (Physical Address Extensions)
                                                        ; CR4.PSE   (Doesn't apply in our case since PAE in enabled.)
                                                        ; PDPE.PS=0 (Page directory pointer offset)
                                                        ; PDE.PS=0  (Page Directory Page Size)
                                                        ; CR0.PG=1  (Paging Enable)
                                                        ;
                                                        ;
                                                        ;This implies :
                                                        ; - 64 bit mode, no 32 bit compatibility.
                                                        ; - Maximum virtual address of 64 bits.
                                                        ; - Maximum physical address of 52 bits.
                                                        ;
                                                        ;
                                                        ; EFER.LME=1
                                                        ;
                                                        ; The steps for enabling long mode are:
                                                        ; Have paging disabled
                                                        ; Set the PAE enable bit in CR4
                                                        ; Load CR3 with the physical address of the PML4
                                                        ; Enable long mode by setting the EFER.LME flag in MSR 0xC0000080
                                                        ; Enable paging
                                                        ;
                                                        ;
                                                        ;
                                                        ; Now the CPU will be in compatibility mode, and instructions are still 32-bit. 
                                                        ; To enter long mode, the D/B bit (bit 22, 2nd dword) of the GDT code segment must be clear (as it would be for a 16-bit code segment), 
                                                        ; and the L bit (bit 21, 2nd dword) of the GDT code segment must be set. 
                                                        ; Once that is done, the CPU is in 64-bit long mode.
                                                        ;
                                                    	; Disable 32 bits paging now!
mov eax, cr0                                            ;
btr eax, 31                                             ; Remove bit 31 of CR0.
mov cr0, eax                                            ;
                                                        ;
mov esi, msg_ensure_paging_off                          ; Informs paging is now disabled.
call asm32_display_writestring                          ;
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
                                                        ; Load CR3 with PML4 address BEFORE enabling PAE
                                                        ; (PAE enabling validates paging structures immediately!)
xor eax, eax                                            ;
mov eax, [PML4T_LOCATION]                               ;
mov cr3, eax                                            ;
                                                        ;
mov esi, msg_cr3_loaded                                 ; Tell CR3 is loaded
call asm32_display_writestring                          ;
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
                                                        ; Set PAE enable bit in CR4 (Physical Address Extensions)
                                                        ;
mov eax, cr4                                            ;
bts eax, 5                                              ; Enable bit 5 of CR4 (Enable PAE)
btr eax, 7                                              ; Disable bit 7 of CR4 (Disable PGE)
mov cr4, eax                                            ;
                                                        ;
mov esi, msg_enable_pae                                 ; Tell PAE is now enabled.
call asm32_display_writestring                          ;
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
                                                        ; Set EFER.LME=1
mov ecx, 0xC0000080                                     ; EFER
rdmsr                                                   ; Read EFER MSR
bts eax, 8                                              ; Set "Long Mode Enable Bit" to 1
wrmsr                                                   ; Write EFER MSR
                                                        ;
mov esi, msg_enable_lme                                 ;
call asm32_display_writestring                          ;
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
                                                        ;
                                                        ; Enable paging
                                                        ; (CR3 already loaded earlier, before PAE was enabled)
                                                        ;
mov esi, msg_about_to_enable_paging                     ; About to enable paging
call asm32_display_writestring                          ;
                                                        ;
cli                                                     ; CRITICAL: Disable interrupts before enabling paging
                                                        ; Hardware interrupts can fire and cause triple fault if handlers aren't mapped
                                                        ;
                                                        ; Enable paging now!
mov eax, cr0                                            ;
bts eax, 31                                             ;
mov cr0, eax                                            ;
                                                        ;
                                                        ; Immediately after enabling paging, do a short jump to flush pipeline
jmp .paging_enabled                                     ;
.paging_enabled:                                        ;
                                                        ;
mov esi, msg_enable_paging                              ; Tell we did it!
call asm32_display_writestring                          ;
                                                        ;
    ;-----------------------------                      ;--------------------------------------------------------------------------------
                                                        ; Jump to 64 bits code.
                                                        ; We are now in compat mode.
lgdt [GDT64.Pointer]                                    ; Load the 64-bit global descriptor table.
jmp GDT64.Code:Realm64                                  ; Set the code segment and enter 64-bit long mode.
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
[BITS 64]                                               ; Now we're talking...
                                                        ;
EXTERN kprint                                           ; C functions
EXTERN init_memmgr                                      ;
EXTERN alloc_bsp_stack                                  ; Allocate BSP stack from high memory
EXTERN serial_init                                      ; Initialize COM1 serial port
EXTERN cls                                              ;
EXTERN update_cursor                                    ;
EXTERN scroll                                           ;
                                                        ;
GLOBAL ap_trampoline_start                              ;
GLOBAL ap_trampoline_end                                ;
                                                        ;
Realm64:                                                ; Arrivals
                                                        ;
jmp include_64bits_functions                            ; include
    %include 'asm64/asm64_display.inc'                  ; ASM64 display functions.
    %include 'asm64/asm64_setup_idt.inc'                ; ASM64 IDT functions.
    %include 'asm64/asm64_syscall.inc'                  ; SYSCALL/SYSRET entry point and MSR setup.
include_64bits_functions:                               ; /include
                                                        ;
    mov ax, GDT64.Data                                  ; Set the A-register to the data descriptor.
    mov ds, ax                                          ; Set the data segment to the A-register.
    mov es, ax                                          ; Set the extra segment to the A-register.
    mov fs, ax                                          ; Set the F-segment to the A-register.
    mov gs, ax                                          ; Set the G-segment to the A-register.
    mov ss, ax                                          ;
    mov rsp, temp_stack_end                     ; Temporary stack until BSP stack allocated
                                                        ;
    mov rsi, msg_64_bits                                ; Tell user we're in 64 bits
    call asm64_display_writestring                      ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
                                                        ;
                                                        ;
    call asm64_setup_idt                                ; Sets up the IDT and the ISRs
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
                                                        ; Display called display functions will now be from the video
                                                        ; driver written in C from drivers/monitor.c
    call update_cursor                                  ;
                                                        ;
    mov rdi, msg_c64_function                           ; ABI Page 21, Figure 3.4, register usage.
    call kprint                                         ; www.x86-64.org/documentation/abi.pdf
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
    call init_memmgr                                    ; Call the memory manager initialization.
    call alloc_bsp_stack                                ; Allocate 64KB BSP stack from high memory
    mov rsp, rax                                        ; Switch to real stack (rax = stack top)
    call serial_init                                    ; Initialize COM1 (output mirrored from here)
    call heap_init                                      ; Initialize heap allocator.
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; SMP Bring-Up
    call acpi_init                                      ; Parse ACPI tables (RSDP/MADT)
    call map_mmio_region                                ; Map LAPIC/IOAPIC MMIO pages
    call disable_pic                                    ; Mask all PIC IRQs
    call lapic_init                                     ; Enable Local APIC on BSP
    call ioapic_init                                    ; Setup I/O APIC routing
    call cpu_init                                       ; Initialize per-CPU structures
    call ap_startup                                     ; Wake APs via INIT-SIPI-SIPI
    call tss_init                                       ; Setup BSP TSS and load Task Register
    call syscall_init                                   ; Setup SYSCALL/SYSRET MSRs and SWAPGS
    call task_init                                      ; Initialize BSP task scheduler
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
    call ide_init                                       ; Initialize IDE controller.
    call fat32_init                                     ; Initialize FAT32 filesystem.
    call pci_init                                       ; Enumerate PCI devices.
    call ahci_init                                      ; Initialize AHCI SATA controller.
    call bootmedia_init                                 ; Discover and mount boot ISO.
    call nic_init                                       ; Initialize NIC drivers.

;----------------------------------------------------------------------------------------------------------------------------------------
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
    ; Create shell task and drop to ring 3
    mov rdi, shell_task_name                             ; arg1 = task name
    mov rsi, shell_ring3_entry                          ; arg2 = entry point
    call task_create                                    ; Create shell as BSP task 0
    call task_run_first                                 ; Drop to ring 3 (does not return)
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
[SECTION .data]

shell_task_name:        db 'shell', 0

msg16_nommap:           db 'K16 - No memory map, dying here.', 0
msg16_mmap_ok:          db 'K16 - Memory map found, continuing.', 0
msg16_tellbios64:       db 'K16 - Telling the BIOS we will run a 64 bit OS.', 0
msg32gdt:               db 'K16 - Loading 32 bits GDT.', 0
msgprot:                db 'K16 - Switching to protected mode.', 0

msg_32_kernel_code:     db '           - Kernel code section (.text) start', 0
msg_32_kernel_data:     db '           - Kernel data section (.data) start', 0
msg_32_kernel_bss:      db '           - Kernel bss section  (.bss) start', 0
msg_32_kernel_end:      db '           - Kernel ends', 0

msg_32_noapic:          db 'K32 ', 0x1A, ' APIC Not supported by this CPU, we die here.', 0
msg_32_apic:            db 'K32 ', 0x1A, ' APIC detected on this CPU, continuing.', 0
msg_32_bits:            db 'K32 ', 0x1A, ' Kernel 32 bits space starting.', 0
msg_a20_enabled:        db 'K32 ', 0x1A, ' A20 Gate enabled, now accessing the whole memory!', 0
msg_check_cpu_64_yes:   db 'K32 ', 0x1A, ' CPU is 64 bits capable, continuing...', 0
msg_check_cpu_64_no:    db 'K32 ', 0x1A, ' CPU is not 64 bits capable, dying here.', 0
msg_ensure_paging_off:     db 'K32 ', 0x1A, ' Did ensure paging is disabled.', 0
msg_cr3_loaded:            db 'K32 ', 0x1A, ' CR3 loaded with PML4 address.', 0
msg_enable_pae:            db 'K32 ', 0x1A, ' PAE Enabled, now accessing more than 4GB of memory', 0
msg_enable_lme:            db 'K32 ', 0x1A, ' Long mode enabled, CPU now accepts 64 bits instructions.', 0
msg_about_to_enable_paging: db 'K32 ', 0x1A, ' About to enable paging...', 0
msg_enable_paging:         db 'K32 ', 0x1A, ' Paging enabled about to jump in true 64 bits code', 0

msg_64_bits:            db 'K64 ', 0x1A, ' Kernel 64 bits enabled and active.', 0

msg_c64_function:       db 'K64 ', 0x1A, ' Processor initialization done.', 10, 0
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; GDT32
gdtptr:                                                 ;
    dw 0                                                ; limit
    dd 0                                                ; base
                                                        ;
gdt:                                                    ;
    dw 0,0,0,0                                          ; null desciptor
    dw 0FFFFh,0,9A00h,0CFh                              ; 32-bit code desciptor (0x8)
    dw 0FFFFh,0,9200h,08Fh                              ; flat data desciptor   (0x10)
    dw 0FFFFh,0,9A00h,0AFh                              ; 64-bit code desciptor (0x18)
gdtend:                                                 ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; GDT64
                                                        ; AMD volume 2, section 4.8
GDT64:                                                  ; Global Descriptor Table (64-bit).
    .Null: equ $ - GDT64                                ; 0x00 - Null descriptor.
    dw 0                                                ; Limit (low).
    dw 0                                                ; Base (low).
    db 0                                                ; Base (middle)
    db 0                                                ; Access.
    db 0                                                ; Granularity.
    db 0                                                ; Base (high).
    .Code: equ $ - GDT64                                ; 0x08 - Ring 0 code.
    dw 0xFFFF                                           ; Limit (low).
    dw 0                                                ; Base (low).
    db 0                                                ; Base (middle)
    db 10011000b                                        ; Access: P=1, DPL=0, S=1, Type=8 (exec).
    db 00101111b                                        ; Granularity: G=0, L=1 (64-bit).
    db 0                                                ; Base (high).
    .Data: equ $ - GDT64                                ; 0x10 - Ring 0 data.
    dw 0xFFFF                                           ; Limit (low).
    dw 0                                                ; Base (low).
    db 0                                                ; Base (middle)
    db 10010010b                                        ; Access: P=1, DPL=0, S=1, Type=2 (r/w).
    db 00001111b                                        ; Granularity.
    db 0                                                ; Base (high).
    .UserCode32: equ $ - GDT64                          ; 0x18 - Ring 3 compat code (SYSRET placeholder).
    dw 0xFFFF                                           ;
    dw 0                                                ;
    db 0                                                ;
    db 11111000b                                        ; Access: P=1, DPL=3, S=1, Type=8 (exec).
    db 11001111b                                        ; Granularity: G=1, D=1 (32-bit compat).
    db 0                                                ;
    .UserData: equ $ - GDT64                            ; 0x20 - Ring 3 data.
    dw 0xFFFF                                           ;
    dw 0                                                ;
    db 0                                                ;
    db 11110010b                                        ; Access: P=1, DPL=3, S=1, Type=2 (r/w).
    db 00001111b                                        ; Granularity.
    db 0                                                ;
    .UserCode64: equ $ - GDT64                          ; 0x28 - Ring 3 64-bit code.
    dw 0xFFFF                                           ;
    dw 0                                                ;
    db 0                                                ;
    db 11111000b                                        ; Access: P=1, DPL=3, S=1, Type=8 (exec).
    db 00101111b                                        ; Granularity: G=0, L=1 (64-bit).
    db 0                                                ;
    .TSS: equ $ - GDT64                                 ; 0x30 - TSS descriptor (16 bytes, filled at runtime).
    dw 0                                                ; Limit [15:0] (patched by tss_init)
    dw 0                                                ; Base [15:0]
    db 0                                                ; Base [23:16]
    db 0                                                ; Type + flags (patched by tss_init)
    db 0                                                ; Limit [19:16] + granularity
    db 0                                                ; Base [31:24]
    dd 0                                                ; Base [63:32]
    dd 0                                                ; Reserved
                                                        ;
    .Pointer:                                           ; The GDT-pointer.
    dw $ - GDT64 - 1                                    ; Limit.
    dq GDT64                                            ; Base.
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ; AP Trampoline binary (copied to 0x8000 at runtime)
ap_trampoline_start:                                    ;
    incbin "ap_trampoline.bin"                          ;
ap_trampoline_end:                                      ;
;----------------------------------------------------------------------------------------------------------------------------------------
; MEMMAP_START is defined as an absolute symbol in link.ld at 0x500
; so the 16-bit init code can reference it with a 16-bit relocation.

[SECTION .bss]
align 16
IDT32_BASE:
IDT64_BASE:
RESB 0x1000

temp_stack_begin:
RESB 0x1000                                             ; 4KB temp stack for 32-bit and early 64-bit
temp_stack_end:

;----------------------------------------------------------------------------------------------------------------------------------------
; /K

