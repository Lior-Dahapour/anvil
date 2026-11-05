/*
 * Exception vector table: where the CPU jumps when an exception happens (Arm ARM D1.4.1.6).
 *
 * VBAR_EL1 holds the table's address. The CPU jumps to VBAR_EL1 + an offset that depends on
 * the exception type (Sync, IRQ, FIQ, SError) and where it came from. That gives 16 entries
 * of 128 bytes each, so the table is 2 KiB, and it must be 2 KiB aligned because
 * VBAR_EL1 bits [10:0] are RES0 (Arm ARM D24.2.213).
 */

/*
 * One table entry. `kind` is the entry number (0-15), so entry N is at offset N * 0x80.
 * Each entry makes room for a trap_frame on the stack, saves x0/x1 into it, puts its own
 * number in x1 and jumps to exception_common. x0 and x1 are saved first because they are
 * the registers this code needs; every other register is still untouched.
 */
.macro vector_entry kind
    .balign 128
    sub     sp, sp, #288        /* room for a trap_frame (288 = 16 * 18, keeps SP aligned) */
    stp     x0, x1, [sp, #0]    /* memory[sp+0] = x0, memory[sp+8] = x1: frame x[0], x[1] */
    mov     x1, #\kind          /* x1 = which entry this is (0-15) */
    b       exception_common
.endm

    .section .text
    .balign 2048
    .global exception_vectors
exception_vectors:
    /* Exception taken from EL1 while using SP_EL0 (the kernel never runs like this). */
    vector_entry 0      /* 0x000  Sync */
    vector_entry 1      /* 0x080  IRQ */
    vector_entry 2      /* 0x100  FIQ */
    vector_entry 3      /* 0x180  SError */

    /* Exception taken from EL1 while using SP_EL1: the kernel itself (EL1h, see boot.S). */
    vector_entry 4      /* 0x200  Sync: kernel bugs (bad address, undefined instruction) */
    vector_entry 5      /* 0x280  IRQ: device interrupts (Milestone 4) */
    vector_entry 6      /* 0x300  FIQ */
    vector_entry 7      /* 0x380  SError: hardware error reports */

    /* Exception taken from EL0 running 64-bit code: user programs (Milestone 6). */
    vector_entry 8      /* 0x400  Sync: system calls (svc) and user faults */
    vector_entry 9      /* 0x480  IRQ */
    vector_entry 10     /* 0x500  FIQ */
    vector_entry 11     /* 0x580  SError */

    /* Exception taken from EL0 running 32-bit code (Anvil does not support 32-bit programs). */
    vector_entry 12     /* 0x600  Sync */
    vector_entry 13     /* 0x680  IRQ */
    vector_entry 14     /* 0x700  FIQ */
    vector_entry 15     /* 0x780  SError */

/*
 * Shared code that every entry jumps to, with the trap_frame half filled (x0, x1) and the
 * entry number in x1. Fills in the rest of the trap_frame (layout: trap_frame.h), then calls
 * exception_handler() in C.
 */
exception_common:
    /* Save x2..x30 into trap_frame.x[2..30]: register N goes to offset N * 8. */
    stp     x2, x3, [sp, #16]
    stp     x4, x5, [sp, #32]
    stp     x6, x7, [sp, #48]
    stp     x8, x9, [sp, #64]
    stp     x10, x11, [sp, #80]
    stp     x12, x13, [sp, #96]
    stp     x14, x15, [sp, #112]
    stp     x16, x17, [sp, #128]
    stp     x18, x19, [sp, #144]
    stp     x20, x21, [sp, #160]
    stp     x22, x23, [sp, #176]
    stp     x24, x25, [sp, #192]
    stp     x26, x27, [sp, #208]
    stp     x28, x29, [sp, #224]
    str     x30, [sp, #240]

    /* Save the sp from before the exception: the entry moved it down by 288. x2 is free
     * because its original value is already saved above. */
    add     x2, sp, #288
    str     x2, [sp, #248]          /* frame->sp */

    /* Save ELR/SPSR (where it happened, CPU state) and ESR/FAR (why, which address). */
    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    stp     x2, x3, [sp, #256]      /* frame->elr, frame->spsr */
    mrs     x2, esr_el1
    mrs     x3, far_el1
    stp     x2, x3, [sp, #272]      /* frame->esr, frame->far */

    /* exception_handler(frame, kind): x0 = the frame (it starts at sp), x1 = kind, set by
     * the entry and not touched since. */
    mov     x0, sp
    bl      exception_handler
    b       .                       /* the handler never returns; stop if it does */