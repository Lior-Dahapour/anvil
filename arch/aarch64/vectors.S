/*
 * Exception vector table: 16 entries of 128 bytes, 2 KiB aligned (Arm ARM D1.4.1.6; VBAR_EL1
 * bits [10:0] are RES0, D24.2.213).
 */

/* Save x0/x1, put the entry number in x1, jump to the shared code. */
.macro vector_entry kind
    .balign 128
    sub     sp, sp, #288        /* sizeof(struct trap_frame) */
    stp     x0, x1, [sp, #0]
    mov     x1, #\kind
    b       exception_common
.endm

    .section .text
    .balign 2048
    .global exception_vectors
exception_vectors:
    /* Current EL with SP_EL0 (unused). */
    vector_entry 0      /* 0x000  Sync */
    vector_entry 1      /* 0x080  IRQ */
    vector_entry 2      /* 0x100  FIQ */
    vector_entry 3      /* 0x180  SError */

    /* Current EL with SP_EL1: the kernel. */
    vector_entry 4      /* 0x200  Sync */
    vector_entry 5      /* 0x280  IRQ */
    vector_entry 6      /* 0x300  FIQ */
    vector_entry 7      /* 0x380  SError */

    /* Lower EL, AArch64: user programs. */
    vector_entry 8      /* 0x400  Sync */
    vector_entry 9      /* 0x480  IRQ */
    vector_entry 10     /* 0x500  FIQ */
    vector_entry 11     /* 0x580  SError */

    /* Lower EL, AArch32 (not supported). */
    vector_entry 12     /* 0x600  Sync */
    vector_entry 13     /* 0x680  IRQ */
    vector_entry 14     /* 0x700  FIQ */
    vector_entry 15     /* 0x780  SError */

/* Fill in the rest of the trap_frame (layout in trap_frame.h) and call the C handler. */
exception_common:
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

    /* sp before the entry pushed the frame. */
    add     x2, sp, #288
    str     x2, [sp, #248]          /* sp */

    mrs     x2, elr_el1
    mrs     x3, spsr_el1
    stp     x2, x3, [sp, #256]      /* elr, spsr */
    mrs     x2, esr_el1
    mrs     x3, far_el1
    stp     x2, x3, [sp, #272]      /* esr, far */

    /* exception_handler(frame, kind); x1 still holds kind. */
    mov     x0, sp
    bl      exception_handler
    b       .
