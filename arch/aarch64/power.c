#include "boot.h"

#include <arch/power.h>
#include <stdint.h>

/* Defined here, written by boot.S. Zero until boot.S stores the real value. */
uint64_t arch_boot_el;

/* PSCI SYSTEM_OFF function ID, SMC32 calling convention (Arm PSCI spec DEN0022, 5.1.10). */
#define PSCI_SYSTEM_OFF 0x84000008U

/*
 * PSCI is a firmware service; we call it with `hvc` (goes to EL2) or `smc` (goes to EL3).
 * QEMU's device tree says which: "hvc" normally, "smc" with virtualization=on, because then EL2
 * is ours and nothing above us answers hvc. That matches the level we were entered at.
 * TODO(dtb): read the /psci "method" property instead of deriving it from arch_boot_el.
 */
_Noreturn void arch_poweroff(void)
{
    register uint64_t x0 __asm__("x0") = PSCI_SYSTEM_OFF;

    if (arch_boot_el == 2) {
        /*
         * SAFETY: we dropped from EL2 with HCR_EL2.TSC = 0, so `smc` is not trapped to EL2
         * and reaches QEMU's PSCI implementation. SYSTEM_OFF does not return on success; if it
         * does return, x0 holds an error code, which we ignore before parking below.
         */
        __asm__ volatile("smc #0" : "+r"(x0) : : "memory");
    } else {
        /*
         * SAFETY: entered at EL1 under QEMU `virt` without virtualization, so QEMU emulates
         * the EL2 PSCI handler and answers `hvc`. Same return behavior as above.
         */
        __asm__ volatile("hvc #0" : "+r"(x0) : : "memory");
    }

    for (;;) {
        /* SAFETY: wfi only pauses this CPU until an interrupt; all are masked, so it sleeps. */
        __asm__ volatile("wfi");
    }
}
