/*
 * Self-tests that run inside the kernel under QEMU. Built only into self-test kernels
 * (`make SELFTEST=NAME`), never into the normal kernel. `make check` builds each one, boots it
 * and checks the serial output for the expected panic message.
 */
#include <anvil/panic.h>
#include <anvil/selftest.h>

#if defined(ANVIL_SELFTEST_ASSERT)

/* A failing KASSERT must panic and name the condition, file and line. */
_Noreturn void selftest_run(void)
{
    int answer = 2 + 2;

    KASSERT(answer == 5);
    kpanic("selftest: KASSERT did not fire");
}

#else
#error "Unknown SELFTEST name"
#endif
