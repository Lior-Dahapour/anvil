#ifndef ANVIL_ARCH_BOOT_H
#define ANVIL_ARCH_BOOT_H

#include <anvil/types.h>

/**
 * Physical address of the device tree from the bootloader. Not validated: the fdt_* functions
 * check it.
 *
 * Context: any, after the arch boot code (always true in kmain).
 */
phys_addr_t arch_boot_dtb_address(void);

#endif
