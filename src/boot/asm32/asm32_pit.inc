
[BITS 32]
                                                        ;
                                                        ;
asm32_setup_pit:                                        ; Setting up 8253 chipset
                                                        ;
                                                        ; Bit 0: (BCP) Binary Counter
                                                        ;     0: Binary
                                                        ;     1: Binary Coded Decimal (BCD)
                                                        ; Bit 1-3: (M0, M1, M2) Operating Mode. See above sections for a description of each.
                                                        ;     000: Mode 0: Interrupt or Terminal Count
                                                        ;     001: Mode 1: Programmable one-shot
                                                        ;     010: Mode 2: Rate Generator
                                                        ;     011: Mode 3: Square Wave Generator
                                                        ;     100: Mode 4: Software Triggered Strobe
                                                        ;     101: Mode 5: Hardware Triggered Strobe
                                                        ;     110: Undefined; Don't use
                                                        ;     111: Undefined; Don't use
                                                        ; Bits 4-5: (RL0, RL1) Read/Load Mode. We are going to read or send data to a counter register
                                                        ;     00: Counter value is latched into an internal control register at the time of the I/O write operation.
                                                        ;     01: Read or Load Least Significant Byte (LSB) only
                                                        ;     10: Read or Load Most Significant Byte (MSB) only
                                                        ;     11: Read or Load LSB first then MSB
                                                        ; Bits 6-7: (SC0-SC1) Select Counter. See above sections for a description of each.
                                                        ;     00: Counter 0
                                                        ;     01: Counter 1
                                                        ;     10: Counter 2
                                                        ;     11: Illegal value
                                                        ;
                                                        ;
    %define x86_pit_clock_rate  1193180
    %define x86_pit_counter_0   0x40
    %define x86_pit_counter_1   0x41
    %define x86_pit_control     0x43


    ; COUNT = input hz / frequency
    mov dx, x86_pit_clock_rate / 100                    ; 100Hz: 10ms

    mov al, 110110b                                     ; See the comments about for reference on the bit set here.
                                                        ; Basically: Binary. Square wave generator, Read or Load LSB first then MSB, Counter 0 (The one attached to IRQ 0)
    out x86_pit_control, al                             ; Command word sent to the PIT.
                                                        ;
    mov ax, dx
    out x86_pit_counter_0, al ; LSB
    xchg ah, al
    out x86_pit_counter_0, al ; MSB


    ret

