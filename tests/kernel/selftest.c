/*
 * Self-tests that run inside the kernel under QEMU. Built only into self-test kernels
 * (`make SELFTEST=NAME`), never into the normal kernel. `make check` builds each one, boots it
 * and checks the serial output for the expected panic message.
 *
 * The exception tests mark the faulting instruction with the global label `selftest_fault`.
 * `make check` looks that label up in the built kernel and requires the dump's ELR to equal
 * it, which proves the trap frame holds the real saved state.
 */
#include "selftest.h"

#include <anvil/panic.h>
#include <stdint.h>

#if defined(ANVIL_SELFTEST_ASSERT)

/* A failing KASSERT must panic and name the condition, file and line. */
_Noreturn void selftest_run(void)
{
    int answer = 2 + 2;

    KASSERT(answer == 5);
    kpanic("selftest: KASSERT did not fire");
}

#elif defined(ANVIL_SELFTEST_BRK)

/*
 * A breakpoint instruction must reach exception_handler() as EC 0x3c (BRK). x9 is set to a
 * known value first, so `make check` can verify the dump shows the real saved registers.
 */
_Noreturn void selftest_run(void)
{
    /* SAFETY: deliberately takes a synchronous exception; the handler never returns here.
     * x9 is declared clobbered, so the compiler keeps nothing of its own in it. */
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

/* A permanently undefined instruction must reach exception_handler() as EC 0x00 (unknown). */
_Noreturn void selftest_run(void)
{
    /* SAFETY: deliberately takes a synchronous exception; the handler never returns here. */
    __asm__ volatile(".global selftest_fault\n"
                     "selftest_fault:\n"
                     "    udf #0\n");
    kpanic("selftest: udf did not trap");
}

#elif defined(ANVIL_SELFTEST_UNALIGNED)

static uint64_t selftest_buffer[2];

/*
 * An unaligned 8-byte load must reach exception_handler() as EC 0x25 (data abort in the
 * kernel): with the MMU off every data access is Device memory, where unaligned accesses
 * always fault (Arm ARM D8.2.12.1, B2.10.2).
 */
_Noreturn void selftest_run(void)
{
    uint64_t value = 0;
    uintptr_t addr = (uintptr_t)&selftest_buffer[0] + 1U;

    /*
     * SAFETY: deliberately faulting access. `addr` is inside selftest_buffer, so nothing
     * outside the kernel image is touched even if the load were allowed; it is 1 byte past
     * an 8-byte aligned address, so the 8-byte load is misaligned and must fault.
     */
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
