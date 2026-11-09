#ifndef ANVIL_ARCH_AARCH64_TRAP_FRAME_H
#define ANVIL_ARCH_AARCH64_TRAP_FRAME_H

#include <stddef.h>
#include <stdint.h>

/**
 * CPU state saved by vectors.S on exception entry. vectors.S uses fixed offsets, checked by the
 * asserts below. A multiple of 16 bytes so SP stays aligned (AAPCS64 6.4.5.1).
 */
struct trap_frame {
    uint64_t x[31]; /* x0..x30 */
    uint64_t sp;    /* SP before the exception */
    uint64_t elr;   /* ELR_EL1 (Arm ARM C5.2.5) */
    uint64_t spsr;  /* SPSR_EL1 (Arm ARM C5.2.21) */
    uint64_t esr;   /* ESR_EL1 (Arm ARM D24.2.44) */
    uint64_t far;   /* FAR_EL1 (Arm ARM D24.2.47) */
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
 * Called by vectors.S for every exception. Prints the registers and panics.
 *
 * @param frame  The saved state on the stack. Never NULL.
 * @param kind   Vector table entry taken (0-15).
 *
 * Context: exception, IRQs masked. Does not return.
 */
_Noreturn void exception_handler(struct trap_frame *frame, uint64_t kind);

#endif
