
[BITS 32]
[SECTION .text]
;----------------------------------------------------------------------------------------------------------------------------------------
                                                        ;
                                                        ; check if long mode is supported
                                                        ; returns eax=1 if long mode is supported
                                                        ; returns eax=0 if not
asm32_cpuid_long_mode_support:                          ;
		push edx                                        ;
		mov eax, 0x80000000	                            ; Extended-function 80000000h.
                                                        ;
		cpuid                                           ; Is largest extended function
		cmp eax, 0x80000000                             ; any function > 80000000h?
		jbe .no_long_mode                               ; If not, no long mode.
                                                        ;
		mov eax, 0x80000001                             ; Extended-function 80000001h.
		cpuid                                           ; Now EDX = extended-features flags.
		bt edx, 29                                      ; Test if long mode is supported.
                                                        ;
		jnc .no_long_mode                               ; Exit if not supported.
		jmp .long_mode_supported                        ;
                                                        ;
	.no_long_mode:                                      ;
		xor eax, eax                                    ;
		jmp .asm32_cpuid_long_mode_support_end          ;
                                                        ;
	.long_mode_supported:                               ;
		mov eax, 1                                      ;
                                                        ;
	.asm32_cpuid_long_mode_support_end:                 ;
		pop edx                                         ;
		ret                                             ;
                                                        ;
;----------------------------------------------------------------------------------------------------------------------------------------

