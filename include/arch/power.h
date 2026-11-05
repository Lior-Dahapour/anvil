#ifndef ANVIL_ARCH_POWER_H
#define ANVIL_ARCH_POWER_H

/**
 * Power the machine off. Under QEMU this makes QEMU exit.
 *
 * If the firmware refuses, the CPU stops in a low-power wait loop instead. Never returns.
 *
 * Context: any.
 */
_Noreturn void arch_poweroff(void);

#endif
