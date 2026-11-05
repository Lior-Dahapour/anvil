#ifndef ANVIL_ARCH_AARCH64_TRAP_FRAME_H
#define ANVIL_ARCH_AARCH64_TRAP_FRAME_H

#include <stddef.h>
#include <stdint.h>

/**
 * The CPU state at the moment an exception was taken, saved on the stack by vectors.S before
 * any C code runs (C would overwrite the registers).
 *
 * vectors.S writes each field at a fixed offset, so the layout below must never change
 * without changing vectors.S too. The _Static_asserts turn any mismatch into a build error.
 * The size is a multiple of 16 so that pushing a frame keeps SP 16-byte aligned (AAPCS64
 * 6.4.5.1).
 */
struct trap_frame {
    uint64_t x[31]; /* General-purpose registers x0..x30 (x29 = frame pointer, x30 = link). */
    uint64_t sp;    /* Stack pointer before the exception (before the frame was pushed). */
    uint64_t elr;   /* ELR_EL1: address of the instruction that was running (Arm ARM C5.2.5). */
    uint64_t spsr;  /* SPSR_EL1: PSTATE before the exception: EL, DAIF (Arm ARM C5.2.21). */
    uint64_t esr;   /* ESR_EL1: why the exception happened (Arm ARM D24.2.44). */
    uint64_t far;   /* FAR_EL1: faulting address, valid for memory faults (Arm ARM D24.2.47). */
};

_Static_assert(sizeof(struct trap_frame) == 288, "trap_frame size must match vectors.S");
_Static_assert(offsetof(struct trap_frame, sp) == 248, "trap_frame.sp offset must match vectors.S");
_Static_assert(offsetof(struct trap_frame, elr) == 256,
               "trap_frame.elr offset must match vectors.S");
_Static_assert(offsetof(struct trap_frame, spsr) == 264,
               "trap_frame.spsr offset must match vectors.S");
_Static_assert(offsetof(struct trap_frame, esr) == 272,
               "trap_frame.esr offset must match vectors.S");
_Static_assert(offsetof(struct trap_frame, far) == 280,
               "trap_frame.far offset must match vectors.S");

/**
 * Called by vectors.S for every exception, with the saved registers.
 *
 * @param frame  The trap_frame on the stack. Never NULL.
 * @param kind   Which vector table entry was taken (0-15).
 *
 * Context: exception, IRQs masked. Does not return.
 */
_Noreturn void exception_handler(struct trap_frame *frame, uint64_t kind);

#endif
