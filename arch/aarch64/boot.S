/*
 * First code that runs. QEMU jumps here with the MMU off, interrupts masked, and no stack.
 * Job: get to EL1, give C a stack and zeroed globals, then call kmain().
 *
 * Exception levels (Arm ARM DDI 0487, D1.1): EL0 = user programs, EL1 = OS kernel,
 * EL2 = hypervisor, EL3 = secure firmware. QEMU starts us at EL1 by default, or at EL2 with
 * `-M virt,virtualization=on` (and real hardware often starts at EL2).
 */

/* SCTLR_EL1 (Arm ARM D24.2.174): MMU, caches and alignment checks off, little-endian.
 * 0x30d00800 sets only bits 11, 20, 22, 23, 28 and 29. On our Cortex-A72 (Armv8.0) they are
 * RES1 (must be written as 1); on newer CPUs they are optional features (EOS, TSCXT, EIS, SPAN,
 * nTLSMD, LSMAOE) whose value 1 keeps the Armv8.0 behaviour. Every other bit is 0 (off). */
#define SCTLR_EL1_MMU_OFF 0x30d00800

/* HCR_EL2.RW, bit 31 (Arm ARM D24.2.61): EL1 runs in 64-bit (AArch64) state. */
#define HCR_EL2_RW (1 << 31)

/* SPSR_EL2 value for the eret into EL1 (Arm ARM C5.2.22): M[3:0] = 0b0101 is EL1h (EL1 using
 * its own stack, SP_EL1), and bits 9..6 (D, A, I, F) set keep every interrupt type masked. */
#define SPSR_EL1H_MASKED 0x3c5

    .section .text.boot, "ax"
    .global _start
_start:
    /* Mask debug, SError, IRQ and FIQ: we have no exception vectors until Milestone 2. */
    msr     daifset, #0xf

    /* CurrentEL bits [3:2] hold the current exception level (Arm ARM C5.2.2). Kept in x19,
     * a callee-saved register, so it survives until we store it below. */
    mrs     x19, CurrentEL
    ubfx    x19, x19, #2, #2

    cmp     x19, #1
    b.eq    in_el1
    cmp     x19, #2
    b.eq    from_el2
    /* EL3 or anything else: unsupported, stop here. */
    b       park

from_el2:
    /* Leave EL2 by faking a "return from exception" into EL1: eret jumps to ELR_EL2 and
     * takes the processor state (level, stack, masks) from SPSR_EL2. */
    mov     x0, #HCR_EL2_RW
    msr     hcr_el2, x0
    mov     x0, #SPSR_EL1H_MASKED
    msr     spsr_el2, x0
    adr     x0, in_el1
    msr     elr_el2, x0
    eret

in_el1:
    /* SCTLR_EL1 is UNKNOWN after reset when we came from EL2, so always set a known value.
     * isb makes sure the new setting applies before the next instruction. */
    movz    x0, #(SCTLR_EL1_MMU_OFF & 0xffff)
    movk    x0, #(SCTLR_EL1_MMU_OFF >> 16), lsl #16
    msr     sctlr_el1, x0
    isb

    /* Tell the CPU where the exception vector table is (VBAR_EL1, Arm ARM D24.2.213), as
     * early as possible so that every later fault reaches exception_handler(). */
    adrp    x0, exception_vectors
    add     x0, x0, :lo12:exception_vectors
    msr     vbar_el1, x0
    isb

    /* Give C a stack. In EL1h, `sp` is SP_EL1. */
    adrp    x0, __stack_top
    add     x0, x0, :lo12:__stack_top
    mov     sp, x0

    /* Zero .bss: C assumes globals without an initializer start at 0. The linker script
     * aligns both ends to 16 bytes, so these aligned 8-byte stores never go out of range. */
    adrp    x0, __bss_start
    add     x0, x0, :lo12:__bss_start
    adrp    x1, __bss_end
    add     x1, x1, :lo12:__bss_end
1:  cmp     x0, x1
    b.hs    2f
    str     xzr, [x0], #8
    b       1b
2:
    /* Only now, after .bss is zeroed, record the level we were entered at. */
    adrp    x0, arch_boot_el
    str     x19, [x0, :lo12:arch_boot_el]

    bl      kmain

    /* kmain never returns; if it somehow does, stop. */
park:
    wfe
    b       park
