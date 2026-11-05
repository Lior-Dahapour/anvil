#include <stddef.h>
#include <stdint.h>

#include "trap_frame.h"
#include <anvil/console.h>
#include <anvil/panic.h>

/* ESR_EL1.EC, bits [31:26]: the exception class (Arm ARM D24.2.44). */
#define ESR_EC_SHIFT 26U
#define ESR_EC_MASK 0x3fU

/* Exception class values (Arm ARM D24.2.44, table of EC encodings). */
#define ESR_EC_UNKNOWN 0x00U       /* Unknown reason, e.g. an undefined instruction (udf). */
#define ESR_EC_WFX 0x01U           /* Trapped WFI or WFE. */
#define ESR_EC_FP_SIMD 0x07U       /* Access to FP/SIMD registers while they are disabled. */
#define ESR_EC_ILLEGAL_STATE 0x0eU /* Illegal execution state, e.g. a bad eret. */
#define ESR_EC_SVC 0x15U           /* SVC instruction from AArch64 (system call). */
#define ESR_EC_SYSREG 0x18U        /* Trapped MSR, MRS or system instruction. */
#define ESR_EC_IABT_LOWER 0x20U    /* Instruction abort from a lower EL (EL0). */
#define ESR_EC_IABT_CURRENT 0x21U  /* Instruction abort at the current EL (kernel). */
#define ESR_EC_PC_ALIGN 0x22U      /* PC alignment fault. */
#define ESR_EC_DABT_LOWER 0x24U    /* Data abort from a lower EL (EL0). */
#define ESR_EC_DABT_CURRENT 0x25U  /* Data abort at the current EL (kernel). */
#define ESR_EC_SP_ALIGN 0x26U      /* SP alignment fault. */
#define ESR_EC_SERROR 0x2fU        /* SError exception. */
#define ESR_EC_BRK 0x3cU           /* BRK instruction from AArch64. */

/*
 * Names for the exception classes the kernel can run into. One slot for every possible EC
 * value (the mask is 6 bits, so 64 slots), so any EC is a valid index. Slots that are not
 * listed are NULL.
 */
static const char *const ec_names[ESR_EC_MASK + 1U] = {
    [ESR_EC_UNKNOWN] = "unknown reason (undefined instruction?)",
    [ESR_EC_WFX] = "trapped WFI/WFE",
    [ESR_EC_FP_SIMD] = "FP/SIMD access trapped",
    [ESR_EC_ILLEGAL_STATE] = "illegal execution state",
    [ESR_EC_SVC] = "SVC (system call)",
    [ESR_EC_SYSREG] = "trapped MSR/MRS/system instruction",
    [ESR_EC_IABT_LOWER] = "instruction abort from EL0",
    [ESR_EC_IABT_CURRENT] = "instruction abort in kernel",
    [ESR_EC_PC_ALIGN] = "PC alignment fault",
    [ESR_EC_DABT_LOWER] = "data abort from EL0",
    [ESR_EC_DABT_CURRENT] = "data abort in kernel",
    [ESR_EC_SP_ALIGN] = "SP alignment fault",
    [ESR_EC_SERROR] = "SError",
    [ESR_EC_BRK] = "BRK (breakpoint instruction)",
};

static const char *exception_class_name(uint64_t esr)
{
    uint64_t ec = (esr >> ESR_EC_SHIFT) & ESR_EC_MASK;
    if (ec_names[ec] == NULL) {
        return "unrecognized";
    }
    return ec_names[ec];
}

_Noreturn void exception_handler(struct trap_frame *frame, uint64_t kind)
{
    kprintf("EXCEPTION: %s (entry %lu)\n", exception_class_name(frame->esr), kind);
    /* The saved general-purpose registers, three per line. */
    for (size_t i = 0; i < 31; i++) {
        kprintf("  x%-2zu %016lx", i, frame->x[i]);
        if (i % 3 == 2) {
            kprintf("\n");
        }
    }
    kprintf("\n");
    kprintf("  ESR  %016lx   FAR  %016lx\n", frame->esr, frame->far);
    kprintf("  ELR  %016lx   SPSR %016lx\n", frame->elr, frame->spsr);
    kprintf("  SP   %016lx\n", frame->sp);
    kpanic("unhandled exception");
}
