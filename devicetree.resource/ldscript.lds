MEMORY
{
    rom (rx) : org = 0x00000000, l = 8M
    ram (rw) : org = 0xdeaddead, l = 0M
}

ENTRY(_rom_start)

SECTIONS
{
    .text 0x1000 : 
    {
        _rom_start = .;

        /* ROM image will start with compressed ExpansionRom structure */
        BYTE(0x91)                          /* ZORROIII Board with 64K ROM */
        BYTE(~0x21)                         /* ProductID */
        BYTE(~0x50)                         /* ZORROIII Board, cannot be shut up */
        BYTE(~0x00)                         /* Reserved, must be 0 */
        SHORT(~0x6d73)                      /* ManufacturerID */
        LONG(~0x04f6403d)                   /* Serial number */
        SHORT(~(_diag_start - _rom_start))  /* Offset to DiagArea */
        LONG(~0x00000000)                   /* Reserved, must be 0 */

        /* ExpansionControl follows ExpansionRom structure immediately */
        BYTE(0x00)                          /* ec_Interrupt */
        BYTE(~0x00)                         /* BaseAddress 31..24 (ZIII) */
        BYTE(~0x00)                         /* BaseAddress 23..16 (ZII and ZIII) */
        BYTE(~0x00)                         /* ShutUp write address */
        LONG(~0x00000000)                   /* Reserved 14..17 */
        LONG(~0x00000000)                   /* Reserved 18..1b */
        LONG(~0x00000000)                   /* Reserved 1c..1f */

        _diag_start = .;

        BYTE(0x90)                          /* DAC_WORDWIDE | DAC_CONFIGTIME */
        BYTE(0x00)                          /* Flags - no flags */
        SHORT(_ramcopy_end - _diag_start)   /* da_Size */
        SHORT(_DiagPoint - _diag_start)     /* DiagPoint offset */
        SHORT(_BootPoint - _diag_start)     /* BootPoint offset */
        SHORT(_deviceName - _diag_start)    /* Offset to device name */
        LONG(0x00000000)                    /* Reserved, must be 0 */

        _code_start = .;

        /* Everything from main.c will be copied to ram together with DiagArea */
        *main.c.o(.text .text.* .rodata .rodata.*)

        _ramcopy_end = .;

        *(.text .text.* .rodata .rodata.*)
        *(.eh_frame)
    } >rom

    _rom_end = .;

    .data : {
        *(.data .data.*)
    } >ram

    .bss : {
        *(.bss)
    } >ram
}