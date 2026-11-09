#ifndef ANVIL_ARCH_POWER_H
#define ANVIL_ARCH_POWER_H

/**
 * Power off (QEMU exits). If the firmware refuses, the CPU idles forever.
 *
 * Context: any. Does not return.
 */
_Noreturn void arch_poweroff(void);

#endif
