#ifndef ANVIL_ARCH_BOOT_H
#define ANVIL_ARCH_BOOT_H

#include <anvil/types.h>

/**
 * Physical address of the device tree the bootloader handed to the kernel. Not validated:
 * pass it to the fdt_* functions, which check it before trusting anything in it.
 *
 * Context: any, after the architecture's boot code has run (always true in kmain).
 */
phys_addr_t arch_boot_dtb_address(void);

#endif
