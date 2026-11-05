/*
 * Linker script: decides where every piece of the kernel lives in memory.
 *
 * QEMU `virt` puts RAM at 0x40000000 and, for an ELF kernel, copies its 1 MiB device tree to
 * the very start of RAM. We start 2 MiB in so the kernel never overlaps the device tree, and
 * so the base is 2 MiB aligned (useful later for large MMU block mappings).
 * TODO(dtb): the RAM base and size come from the device tree's /memory node from Milestone 3.
 *
 * The MMU is off, so these addresses are physical addresses. QEMU loads each segment at the
 * address written here and jumps to ENTRY.
 */

ENTRY(_start)

KERNEL_BASE = 0x40200000;
KERNEL_STACK_SIZE = 16K;

SECTIONS
{
    . = KERNEL_BASE;

    /* .text.boot first, so _start is the first instruction of the image. */
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

    /*
     * Zero-initialized globals. NOLOAD: nothing is stored in the file, boot.S zeroes this
     * range. Start and end are 16-byte aligned so boot.S can clear it with aligned 8-byte
     * stores (required while the MMU is off, see CLAUDE.md "MMU off in early boot").
     */
    .bss (NOLOAD) : ALIGN(16) {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __bss_end = .;
    }

    /* Boot stack. The AArch64 stack grows down, so SP starts at the top. 16-byte aligned
     * because the calling convention requires SP mod 16 = 0 (AAPCS64 6.4.5.1), and the CPU
     * can fault on a misaligned SP when SCTLR_EL1.SA is set (Arm ARM D1.4.10.2). */
    .stack (NOLOAD) : ALIGN(16) {
        . += KERNEL_STACK_SIZE;
        __stack_top = .;
    }

    /DISCARD/ : {
        *(.comment)
        *(.note .note.*)
        *(.eh_frame .eh_frame_hdr)
    }
}
