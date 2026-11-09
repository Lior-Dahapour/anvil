/*
 * Entry point. QEMU jumps here with the MMU off and no stack. Drop to EL1 if needed, set up
 * the stack and .bss, then call kmain().
 */

/* SCTLR_EL1 (Arm ARM D24.2.174): MMU, caches and alignment checks off, little-endian. Bits 11,
 * 20, 22, 23, 28 and 29 are RES1 on Armv8.0 and keep the Armv8.0 behaviour on newer CPUs. */
#define SCTLR_EL1_MMU_OFF 0x30d00800

/* HCR_EL2.RW (Arm ARM D24.2.61): EL1 is AArch64. */
#define HCR_EL2_RW (1 << 31)

/* SPSR_EL2 for the eret: EL1h, DAIF masked (Arm ARM C5.2.22). */
#define SPSR_EL1H_MASKED 0x3c5

    .section .text.boot, "ax"
    .global _start
_start:
    /* arm64 Image header, so QEMU passes the DTB in x0 (Linux
     * Documentation/arch/arm64/booting.rst, section 4). */
    b       boot_entry          /* code0 */
    .long   0                   /* code1 */
    .quad   0                   /* text_offset */
    .quad   __image_size        /* image_size, from link.ld */
    .quad   0xa                 /* flags: little-endian, 4 KiB pages, any 2 MiB base */
    .quad   0                   /* res2 */
    .quad   0                   /* res3 */
    .quad   0                   /* res4 */
    .long   0x644d5241          /* magic: "ARM\x64" */
    .long   0                   /* res5 */

boot_entry:
    /* Keep the DTB address in x20; x0 is scratch below. */
    mov     x20, x0

    /* No interrupt handling yet. */
    msr     daifset, #0xf

    /* CurrentEL[3:2] (Arm ARM C5.2.2). */
    mrs     x19, CurrentEL
    ubfx    x19, x19, #2, #2

    cmp     x19, #1
    b.eq    in_el1
    cmp     x19, #2
    b.eq    from_el2
    /* EL3 is not supported. */
    b       park

from_el2:
    mov     x0, #HCR_EL2_RW
    msr     hcr_el2, x0
    mov     x0, #SPSR_EL1H_MASKED
    msr     spsr_el2, x0
    adr     x0, in_el1
    msr     elr_el2, x0
    eret

in_el1:
    /* SCTLR_EL1 is UNKNOWN when we come from EL2. */
    movz    x0, #(SCTLR_EL1_MMU_OFF & 0xffff)
    movk    x0, #(SCTLR_EL1_MMU_OFF >> 16), lsl #16
    msr     sctlr_el1, x0
    isb

    /* Install the vectors (VBAR_EL1, Arm ARM D24.2.213) early, so every fault is reported. */
    adrp    x0, exception_vectors
    add     x0, x0, :lo12:exception_vectors
    msr     vbar_el1, x0
    isb

    /* In EL1h, sp is SP_EL1. */
    adrp    x0, __stack_top
    add     x0, x0, :lo12:__stack_top
    mov     sp, x0

    /* Zero .bss. link.ld aligns both ends to 16 bytes. */
    adrp    x0, __bss_start
    add     x0, x0, :lo12:__bss_start
    adrp    x1, __bss_end
    add     x1, x1, :lo12:__bss_end
1:  cmp     x0, x1
    b.hs    2f
    str     xzr, [x0], #8
    b       1b
2:
    /* After zeroing .bss, which holds these. */
    adrp    x0, arch_boot_el
    str     x19, [x0, :lo12:arch_boot_el]
    adrp    x0, arch_boot_dtb
    str     x20, [x0, :lo12:arch_boot_dtb]

    bl      kmain

park:
    wfe
    b       park
