#ifndef ANVIL_ARCH_AARCH64_BOOT_H
#define ANVIL_ARCH_AARCH64_BOOT_H

#include <stdint.h>

/**
 * Exception level the CPU was in when QEMU started the kernel (1 or 2).
 *
 * Written once by boot.S after .bss is zeroed and before kmain() runs; read-only afterwards.
 *
 * Context: any.
 */
extern uint64_t arch_boot_el;

#endif
