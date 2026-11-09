/*
 * In-kernel crash tests, built only with `make SELFTEST=NAME`. `make check` boots each one and
 * checks the output. Fault tests label the faulting instruction `selftest_fault`, and the dump's
 * ELR must equal it.
 */
#include "selftest.h"

#include <anvil/panic.h>
#include <stdint.h>

#if defined(ANVIL_SELFTEST_ASSERT)

/* KASSERT must panic with the condition, file and line. */
_Noreturn void selftest_run(void)
{
    int answer = 2 + 2;

    KASSERT(answer == 5);
    kpanic("selftest: KASSERT did not fire");
}

#elif defined(ANVIL_SELFTEST_BRK)

/* brk must trap as EC 0x3c. `make check` looks for x9 = 0x1234 in the dump. */
_Noreturn void selftest_run(void)
{
    /* SAFETY: traps on purpose, the handler does not return. x9 is declared clobbered. */
    __asm__ volatile("    mov x9, #0x1234\n"
                     ".global selftest_fault\n"
                     "selftest_fault:\n"
                     "    brk #0\n"
                     :
                     :
                     : "x9");
    kpanic("selftest: brk did not trap");
}

#elif defined(ANVIL_SELFTEST_UDF)

/* udf must trap as EC 0x00 (unknown). */
_Noreturn void selftest_run(void)
{
    /* SAFETY: traps on purpose, the handler does not return. */
    __asm__ volatile(".global selftest_fault\n"
                     "selftest_fault:\n"
                     "    udf #0\n");
    kpanic("selftest: udf did not trap");
}

#elif defined(ANVIL_SELFTEST_UNALIGNED)

static uint64_t selftest_buffer[2];

/* An unaligned load must fault as EC 0x25: with the MMU off all memory is Device, where
 * unaligned accesses fault (Arm ARM D8.2.12.1, B2.10.2). */
_Noreturn void selftest_run(void)
{
    uint64_t value = 0;
    uintptr_t addr = (uintptr_t)&selftest_buffer[0] + 1U;

    /* SAFETY: faults on purpose. `addr` is inside selftest_buffer, so even a completed load
     * stays in bounds. */
    __asm__ volatile(".global selftest_fault\n"
                     "selftest_fault:\n"
                     "    ldr %0, [%1]\n"
                     : "=r"(value)
                     : "r"(addr)
                     : "memory");
    kpanic("selftest: unaligned load did not fault (read %lx)", value);
}

#else
#error "Unknown SELFTEST name"
#endif
