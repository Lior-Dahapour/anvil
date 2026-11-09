#ifndef ANVIL_ARCH_AARCH64_BOOT_H
#define ANVIL_ARCH_AARCH64_BOOT_H

#include <stdint.h>

/**
 * Exception level at entry (1 or 2). Set by boot.S before kmain(), read-only after.
 *
 * Context: any.
 */
extern uint64_t arch_boot_el;

/**
 * Physical DTB address from x0, not validated. Set by boot.S before kmain(), read-only after.
 *
 * Context: any.
 */
extern uint64_t arch_boot_dtb;

#endif
