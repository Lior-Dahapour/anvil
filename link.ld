/*
 * Linked 2 MiB into RAM (RAM starts at 0x40000000), where QEMU loads the Image. The MMU is
 * off, so these are physical addresses.
 */

ENTRY(_start)

KERNEL_BASE = 0x40200000;
KERNEL_STACK_SIZE = 16K;

SECTIONS
{
    . = KERNEL_BASE;
    __image_start = .;

    /* .text.boot first, so the image starts with _start. */
    .text : {
        KEEP(*(.text.boot))
        *(.text .text.*)
    }

    .rodata : ALIGN(16) {
        *(.rodata .rodata.*)
    }

    .data : ALIGN(16) {
        *(.data .data.*)
    }

    /* boot.S zeroes this with 8-byte stores, so both ends are 16-byte aligned. */
    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    }

    /* SP must be 16-byte aligned (AAPCS64 6.4.5.1, Arm ARM D1.4.10.2). */
    .stack (NOLOAD) : ALIGN(16) {
        . += KERNEL_STACK_SIZE;
        __stack_top = .;
    }

    /* Whole footprint including .bss and stack, for the image header in boot.S. */
    __image_end = .;
    __image_size = __image_end - KERNEL_BASE;

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame_hdr)
    }
}
