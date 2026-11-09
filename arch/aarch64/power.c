#include "boot.h"

#include <arch/power.h>
#include <stdint.h>

/* PSCI SYSTEM_OFF, SMC32 convention (PSCI DEN0022, 5.1.9). */
#define PSCI_SYSTEM_OFF 0x84000008U

/*
 * QEMU's PSCI is reached with hvc, or with smc under virtualization=on (EL2 is then ours).
 * TODO(dtb): read the /psci "method" property instead of using arch_boot_el.
 */
_Noreturn void arch_poweroff(void)
{
    register uint64_t x0 __asm__("x0") = PSCI_SYSTEM_OFF;

    if (arch_boot_el == 2) {
        /*
         * SAFETY: boot.S left HCR_EL2.TSC = 0, so smc is not trapped and reaches QEMU's PSCI.
         * Returns only on failure, then we park below.
         */
        __asm__ volatile("smc #0" : "+r"(x0) : : "memory");
    } else {
        /* SAFETY: without virtualization QEMU answers hvc as PSCI. Returns only on failure. */
        __asm__ volatile("hvc #0" : "+r"(x0) : : "memory");
    }

    for (;;) {
        /* SAFETY: wfi only idles the CPU; the loop covers wakeups. */
        __asm__ volatile("wfi");
    }
}
